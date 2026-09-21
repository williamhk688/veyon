/*
 * WindowsWolAdapterControl.cpp - pick an Ethernet adapter for WOL, temporarily
 * enable it via Veyon Service if needed, and restore the previous state
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <winsock2.h>
#include <windows.h>
#include <netioapi.h>
#include <iphlpapi.h>
#include <ws2ipdef.h>
#include <setupapi.h>
#include <devguid.h>
#include <sddl.h>

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QMutex>
#include <QThread>
#include <QUuid>

#include "VeyonCore.h"
#include "WindowsWolAdapterControl.h"
#include "WindowsWolAdapterIpc.h"

#ifndef GAA_FLAG_INCLUDE_ALL_INTERFACES
#define GAA_FLAG_INCLUDE_ALL_INTERFACES 0x0100
#endif

namespace
{

quint32 prefixToMask(int prefixLength)
{
	if (prefixLength <= 0)
	{
		return 0;
	}
	if (prefixLength >= 32)
	{
		return 0xffffffffu;
	}
	return (~quint32(0)) << (32 - prefixLength);
}

QHostAddress directedBroadcast(const QHostAddress& ipv4, int prefixLength)
{
	if (ipv4.protocol() != QAbstractSocket::IPv4Protocol)
	{
		return {};
	}

	return QHostAddress(ipv4.toIPv4Address() | ~prefixToMask(prefixLength));
}

bool subnetContains(const QHostAddress& networkIp, int prefixLength, const QHostAddress& host)
{
	if (networkIp.protocol() != QAbstractSocket::IPv4Protocol ||
		host.protocol() != QAbstractSocket::IPv4Protocol)
	{
		return false;
	}

	const quint32 mask = prefixToMask(prefixLength);
	return (networkIp.toIPv4Address() & mask) == (host.toIPv4Address() & mask);
}

struct AdapterInfo
{
	ULONG index = 0;
	IFTYPE type = 0;
	NET_IF_ADMIN_STATUS adminStatus = NET_IF_ADMIN_STATUS_UP;
	IF_OPER_STATUS operStatus = IfOperStatusDown;
	bool hardware = false;
	QString alias;
	QString description;
	QHostAddress ipv4;
	int prefixLength = 0;
};

AdapterInfo adapterByIndex(unsigned long interfaceIndex);

bool isWifi(IFTYPE type)
{
	return type == IF_TYPE_IEEE80211;
}

bool isEthernet(IFTYPE type)
{
	return type == IF_TYPE_ETHERNET_CSMACD;
}

bool isIgnoredType(IFTYPE type)
{
	return type == IF_TYPE_SOFTWARE_LOOPBACK ||
			type == IF_TYPE_TUNNEL ||
			type == IF_TYPE_PPP ||
			isWifi(type);
}

bool looksVirtual(const AdapterInfo& adapter)
{
	const auto name = (adapter.alias + QLatin1Char(' ') + adapter.description).toLower();
	return name.contains(QLatin1String("vethernet")) ||
			name.contains(QLatin1String("hyper-v")) ||
			name.contains(QLatin1String("vmware")) ||
			name.contains(QLatin1String("virtualbox")) ||
			name.contains(QLatin1String("virtual")) ||
			name.contains(QLatin1String("vpn")) ||
			name.contains(QLatin1String("tap-")) ||
			name.contains(QLatin1String("bluetooth")) ||
			name.contains(QLatin1String("teredo")) ||
			name.contains(QLatin1String("isatap")) ||
			name.contains(QLatin1String("wi-fi direct"));
}

QList<AdapterInfo> enumerateAdapters()
{
	QList<AdapterInfo> adapters;

	PMIB_IF_TABLE2 table = nullptr;
	if (GetIfTable2(&table) != NO_ERROR || table == nullptr)
	{
		return adapters;
	}

	for (ULONG i = 0; i < table->NumEntries; ++i)
	{
		const MIB_IF_ROW2& row = table->Table[i];
		if (isIgnoredType(row.Type))
		{
			continue;
		}

		AdapterInfo adapter;
		adapter.index = row.InterfaceIndex;
		adapter.type = row.Type;
		adapter.adminStatus = row.AdminStatus;
		adapter.operStatus = row.OperStatus;
		adapter.hardware = row.InterfaceAndOperStatusFlags.HardwareInterface != 0;
		adapter.alias = QString::fromWCharArray(row.Alias);
		adapter.description = QString::fromWCharArray(row.Description);
		adapters.append(adapter);
	}

	FreeMibTable(table);

	ULONG addressSize = 0;
	GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_ALL_INTERFACES,
						 nullptr, nullptr, &addressSize);
	if (addressSize == 0)
	{
		return adapters;
	}

	QByteArray buffer(int(addressSize), 0);
	auto addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
	if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_ALL_INTERFACES,
							 nullptr, addresses, &addressSize) != NO_ERROR)
	{
		return adapters;
	}

	for (auto current = addresses; current != nullptr; current = current->Next)
	{
		for (auto& adapter : adapters)
		{
			if (adapter.index != current->IfIndex)
			{
				continue;
			}

			if (adapter.description.isEmpty() && current->Description)
			{
				adapter.description = QString::fromWCharArray(current->Description);
			}

			for (auto unicast = current->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
			{
				if (unicast->Address.lpSockaddr == nullptr ||
					unicast->Address.lpSockaddr->sa_family != AF_INET)
				{
					continue;
				}

				const auto* ipv4 = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
				adapter.ipv4 = QHostAddress(ntohl(ipv4->sin_addr.S_un.S_addr));
				adapter.prefixLength = int(unicast->OnLinkPrefixLength);
				break;
			}
		}
	}

	return adapters;
}

bool adapterMatchesTargets(const AdapterInfo& adapter, const QList<QHostAddress>& targets)
{
	if (adapter.ipv4.isNull() || adapter.prefixLength <= 0)
	{
		return false;
	}

	for (const auto& target : targets)
	{
		if (subnetContains(adapter.ipv4, adapter.prefixLength, target))
		{
			return true;
		}
	}

	return false;
}

AdapterInfo chooseEthernetAdapter(const QList<AdapterInfo>& adapters, const QList<QHostAddress>& targets)
{
	QList<AdapterInfo> ethernet;
	ethernet.reserve(adapters.size());
	for (const auto& adapter : adapters)
	{
		if (isEthernet(adapter.type) && adapter.hardware && looksVirtual(adapter) == false)
		{
			ethernet.append(adapter);
		}
	}

	if (ethernet.isEmpty())
	{
		for (const auto& adapter : adapters)
		{
			if (isEthernet(adapter.type) && looksVirtual(adapter) == false)
			{
				ethernet.append(adapter);
			}
		}
	}

	if (ethernet.isEmpty())
	{
		for (const auto& adapter : adapters)
		{
			if (isEthernet(adapter.type))
			{
				ethernet.append(adapter);
			}
		}
	}

	auto firstMatch = [&](const auto& predicate) -> AdapterInfo {
		for (const auto& adapter : ethernet)
		{
			if (predicate(adapter))
			{
				return adapter;
			}
		}
		return {};
	};

	if (auto match = firstMatch([&](const AdapterInfo& adapter) {
			return adapter.operStatus == IfOperStatusUp && adapterMatchesTargets(adapter, targets);
		}); match.index != 0)
	{
		return match;
	}

	if (auto match = firstMatch([&](const AdapterInfo& adapter) {
			return adapterMatchesTargets(adapter, targets);
		}); match.index != 0)
	{
		return match;
	}

	if (auto match = firstMatch([](const AdapterInfo& adapter) {
			return adapter.adminStatus == NET_IF_ADMIN_STATUS_DOWN;
		}); match.index != 0)
	{
		return match;
	}

	if (auto match = firstMatch([](const AdapterInfo& adapter) {
			return adapter.operStatus == IfOperStatusUp && adapter.ipv4.isNull() == false;
		}); match.index != 0)
	{
		return match;
	}

	if (ethernet.isEmpty() == false)
	{
		return ethernet.first();
	}

	return {};
}

bool setIfEntryAdminStatus(unsigned long interfaceIndex, bool enabled)
{
	MIB_IFROW row{};
	row.dwIndex = interfaceIndex;
	if (GetIfEntry(&row) != NO_ERROR)
	{
		vWarning() << "GetIfEntry failed for interface" << interfaceIndex << GetLastError();
		return false;
	}

	row.dwAdminStatus = enabled ? MIB_IF_ADMIN_STATUS_UP : MIB_IF_ADMIN_STATUS_DOWN;
	const auto error = SetIfEntry(&row);
	if (error != NO_ERROR)
	{
		vWarning() << "SetIfEntry failed for interface" << interfaceIndex << "error" << error;
		return false;
	}

	return true;
}

bool adapterAdminMatches(unsigned long interfaceIndex, bool enabled, int timeoutMs)
{
	QElapsedTimer timer;
	timer.start();
	AdapterInfo adapter;
	while (timer.elapsed() < timeoutMs)
	{
		adapter = adapterByIndex(interfaceIndex);
		const bool isUp = adapter.index != 0 && adapter.adminStatus == NET_IF_ADMIN_STATUS_UP;
		if (isUp == enabled)
		{
			return true;
		}
		QThread::msleep(50);
	}

	WindowsWolAdapterControl::log(QStringLiteral("status mismatch %1 wantEnabled %2 admin %3 oper %4 alias %5")
								  .arg(interfaceIndex)
								  .arg(enabled)
								  .arg(adapter.adminStatus)
								  .arg(adapter.operStatus)
								  .arg(adapter.alias));
	return false;
}

void logAdapters(const QString& reason)
{
	WindowsWolAdapterControl::log(reason);
	const auto adapters = enumerateAdapters();
	if (adapters.isEmpty())
	{
		WindowsWolAdapterControl::log(QStringLiteral("adapter list empty"));
		return;
	}

	for (const auto& adapter : adapters)
	{
		WindowsWolAdapterControl::log(
			QStringLiteral("adapter %1 type %2 hw %3 virtual %4 admin %5 oper %6 %7 | %8")
				.arg(adapter.index)
				.arg(adapter.type)
				.arg(adapter.hardware)
				.arg(looksVirtual(adapter))
				.arg(adapter.adminStatus)
				.arg(adapter.operStatus)
				.arg(adapter.alias)
				.arg(adapter.description));
	}
}

bool setSetupDiAdminStatus(unsigned long interfaceIndex, bool enabled)
{
	NET_LUID luid{};
	if (ConvertInterfaceIndexToLuid(interfaceIndex, &luid) != NO_ERROR)
	{
		return false;
	}

	GUID guid{};
	if (ConvertInterfaceLuidToGuid(&luid, &guid) != NO_ERROR)
	{
		return false;
	}

	const auto wantedGuid = QUuid(guid).toString();

	const HDEVINFO deviceInfoSet =
			SetupDiGetClassDevsW(&GUID_DEVCLASS_NET, nullptr, nullptr, DIGCF_PRESENT);
	if (deviceInfoSet == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	bool success = false;
	SP_DEVINFO_DATA deviceInfoData{};
	deviceInfoData.cbSize = sizeof(deviceInfoData);

	for (DWORD index = 0; SetupDiEnumDeviceInfo(deviceInfoSet, index, &deviceInfoData); ++index)
	{
		HKEY key = SetupDiOpenDevRegKey(deviceInfoSet, &deviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
		if (key == INVALID_HANDLE_VALUE)
		{
			continue;
		}

		wchar_t value[128] = {};
		DWORD valueSize = sizeof(value);
		DWORD type = 0;
		const auto query = RegQueryValueExW(key, L"NetCfgInstanceId", nullptr, &type,
											reinterpret_cast<LPBYTE>(value), &valueSize);
		RegCloseKey(key);
		if (query != ERROR_SUCCESS)
		{
			continue;
		}

		if (QString::fromWCharArray(value).compare(wantedGuid, Qt::CaseInsensitive) != 0)
		{
			continue;
		}

		SP_PROPCHANGE_PARAMS params{};
		params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
		params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
		params.StateChange = enabled ? DICS_ENABLE : DICS_DISABLE;
		params.Scope = DICS_FLAG_GLOBAL;
		params.HwProfile = 0;

		if (SetupDiSetClassInstallParamsW(deviceInfoSet, &deviceInfoData,
										  &params.ClassInstallHeader, sizeof(params)) &&
			SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, deviceInfoSet, &deviceInfoData))
		{
			success = true;
		}
		break;
	}

	SetupDiDestroyDeviceInfoList(deviceInfoSet);
	return success;
}

bool setNetshAdminStatus(const QString& alias, bool enabled)
{
	if (alias.isEmpty())
	{
		return false;
	}

	wchar_t systemRoot[MAX_PATH] = {};
	if (GetEnvironmentVariableW(L"SystemRoot", systemRoot, MAX_PATH) == 0)
	{
		wcsncpy(systemRoot, L"C:\\Windows", MAX_PATH - 1);
	}

	const QString command = QStringLiteral("\"%1\\System32\\netsh.exe\" interface set interface name=\"%2\" admin=%3")
							.arg(QString::fromWCharArray(systemRoot),
								 alias,
								 enabled ? QStringLiteral("ENABLED") : QStringLiteral("DISABLED"));
	wchar_t commandLine[1024] = {};
	const int length = command.toWCharArray(commandLine);
	if (length <= 0 || length >= int(sizeof(commandLine) / sizeof(commandLine[0])) - 1)
	{
		return false;
	}
	commandLine[length] = 0;

	STARTUPINFOW startupInfo{};
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION processInfo{};
	if (CreateProcessW(nullptr, commandLine, nullptr, nullptr, FALSE,
					   CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo) == FALSE)
	{
		WindowsWolAdapterControl::log(QStringLiteral("netsh CreateProcess failed %1 for %2").arg(GetLastError()).arg(alias));
		return false;
	}

	const DWORD waited = WaitForSingleObject(processInfo.hProcess, 8000);
	DWORD exitCode = 1;
	GetExitCodeProcess(processInfo.hProcess, &exitCode);
	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);

	WindowsWolAdapterControl::log(QStringLiteral("netsh %1 admin=%2 wait=%3 exit=%4")
								  .arg(alias, enabled ? QStringLiteral("ENABLED") : QStringLiteral("DISABLED"))
								  .arg(waited)
								  .arg(exitCode));
	return waited == WAIT_OBJECT_0 && exitCode == 0;
}

AdapterInfo adapterByIndex(unsigned long interfaceIndex)
{
	const auto adapters = enumerateAdapters();
	for (const auto& adapter : adapters)
	{
		if (adapter.index == interfaceIndex)
		{
			return adapter;
		}
	}
	return {};
}

class WindowsWakeOnLanSession : public PlatformNetworkFunctions::WakeOnLanSession
{
public:
	explicit WindowsWakeOnLanSession(const QList<QHostAddress>& targetHosts)
	{
		s_mutex.lock();
		prepare(targetHosts);
	}

	~WindowsWakeOnLanSession() override
	{
		restore();
		s_mutex.unlock();
	}

private:
	static QMutex s_mutex;

	ULONG m_index = 0;
	NET_IF_ADMIN_STATUS m_originalAdmin = NET_IF_ADMIN_STATUS_UP;
	bool m_changed = false;

	void prepare(const QList<QHostAddress>& targetHosts)
	{
		auto adapter = chooseEthernetAdapter(enumerateAdapters(), targetHosts);
		logAdapters(QStringLiteral("Power On adapter scan"));
		if (adapter.index == 0)
		{
			vWarning() << "no Ethernet adapter found for Wake-on-LAN";
			WindowsWolAdapterControl::log(QStringLiteral("no Ethernet adapter found"));
			return;
		}

		m_index = adapter.index;
		m_originalAdmin = adapter.adminStatus;

		vDebug() << "WOL using interface" << adapter.index << adapter.alias
				 << "admin" << adapter.adminStatus << "oper" << adapter.operStatus
				 << adapter.ipv4.toString();

		WindowsWolAdapterControl::log(QStringLiteral("using interface %1 %2 admin %3 oper %4 ipv4 %5")
									  .arg(adapter.index)
									  .arg(adapter.alias)
									  .arg(adapter.adminStatus)
									  .arg(adapter.operStatus)
									  .arg(adapter.ipv4.toString()));

		if (adapter.adminStatus != NET_IF_ADMIN_STATUS_UP)
		{
			if (WindowsWolAdapterControl::setAdminStatus(adapter.index, true) == false)
			{
				vWarning() << "failed to enable WOL interface" << adapter.index;
				WindowsWolAdapterControl::log(QStringLiteral("failed to enable interface %1").arg(adapter.index));
				return;
			}
			m_changed = true;
			WindowsWolAdapterControl::log(QStringLiteral("enabled interface %1").arg(adapter.index));
		}

		QElapsedTimer timer;
		timer.start();
		AdapterInfo ready;
		bool readyOk = false;
		while (timer.elapsed() < WindowsWolAdapterControl::LinkReadyTimeoutMs)
		{
			ready = adapterByIndex(m_index);
			readyOk = ready.adminStatus == NET_IF_ADMIN_STATUS_UP &&
					ready.operStatus == IfOperStatusUp &&
					ready.ipv4.isNull() == false &&
					ready.prefixLength > 0 &&
					ready.prefixLength < 32;
			if (readyOk)
			{
				break;
			}
			QThread::msleep(WindowsWolAdapterControl::LinkPollIntervalMs);
		}

		if (readyOk == false)
		{
			vWarning() << "WOL interface" << m_index << "did not become ready in time"
					   << "admin" << ready.adminStatus << "oper" << ready.operStatus
					   << "IPv4" << ready.ipv4.toString() << "prefix" << ready.prefixLength;
			WindowsWolAdapterControl::log(QStringLiteral("interface %1 not ready admin %2 oper %3 ipv4 %4")
										  .arg(m_index)
										  .arg(ready.adminStatus)
										  .arg(ready.operStatus)
										  .arg(ready.ipv4.toString()));
			return;
		}

		m_endpoint.localAddress = ready.ipv4;
		m_endpoint.broadcastAddress = directedBroadcast(ready.ipv4, ready.prefixLength);
		m_endpoint.interfaceIndex = int(m_index);

		vDebug() << "WOL endpoint" << m_endpoint.localAddress.toString()
				 << "broadcast" << m_endpoint.broadcastAddress.toString()
				 << "index" << m_endpoint.interfaceIndex;
	}

	void restore()
	{
		if (m_changed == false || m_index == 0)
		{
			return;
		}

		const bool enable = m_originalAdmin == NET_IF_ADMIN_STATUS_UP;
		if (WindowsWolAdapterControl::setAdminStatus(m_index, enable) == false)
		{
			vWarning() << "failed to restore WOL interface" << m_index << "enabled" << enable;
			WindowsWolAdapterControl::log(QStringLiteral("failed to restore interface %1 enabled %2").arg(m_index).arg(enable));
		}
		else
		{
			vDebug() << "restored WOL interface" << m_index << "enabled" << enable;
			WindowsWolAdapterControl::log(QStringLiteral("restored interface %1 enabled %2").arg(m_index).arg(enable));
		}
		m_changed = false;
	}
};

QMutex WindowsWakeOnLanSession::s_mutex;

}

std::unique_ptr<PlatformNetworkFunctions::WakeOnLanSession>
WindowsWolAdapterControl::acquireSession(const QList<QHostAddress>& targetHosts)
{
	return std::make_unique<WindowsWakeOnLanSession>(targetHosts);
}

bool WindowsWolAdapterControl::canTemporarilyEnableAdapter(unsigned long interfaceIndex)
{
	const auto adapter = adapterByIndex(interfaceIndex);
	return adapter.index != 0 &&
			isEthernet(adapter.type) &&
			looksVirtual(adapter) == false;
}


bool WindowsWolAdapterControl::setAdminStatusNative(unsigned long interfaceIndex, bool enabled)
{
	const auto before = adapterByIndex(interfaceIndex);
	log(QStringLiteral("native %1 %2 | %3 wantEnabled %4 admin %5")
		.arg(interfaceIndex)
		.arg(before.alias)
		.arg(before.description)
		.arg(enabled)
		.arg(before.adminStatus));

	if (setIfEntryAdminStatus(interfaceIndex, enabled) && adapterAdminMatches(interfaceIndex, enabled, 1000))
	{
		log(QStringLiteral("SetIfEntry verified %1 enabled %2").arg(interfaceIndex).arg(enabled));
		return true;
	}

	if (setSetupDiAdminStatus(interfaceIndex, enabled) && adapterAdminMatches(interfaceIndex, enabled, 1500))
	{
		log(QStringLiteral("SetupDi verified %1 enabled %2").arg(interfaceIndex).arg(enabled));
		return true;
	}

	if (setNetshAdminStatus(before.alias, enabled) && adapterAdminMatches(interfaceIndex, enabled, 2000))
	{
		log(QStringLiteral("netsh verified %1 enabled %2").arg(interfaceIndex).arg(enabled));
		return true;
	}

	log(QStringLiteral("native enable/disable failed for %1 enabled %2 lastError %3")
		.arg(interfaceIndex)
		.arg(enabled)
		.arg(GetLastError()));
	return false;
}

bool WindowsWolAdapterControl::setAdminStatus(unsigned long interfaceIndex, bool enabled)
{
	if (setAdminStatusNative(interfaceIndex, enabled))
	{
		return true;
	}

	return WindowsWolAdapterIpcClient::setAdminStatus(interfaceIndex, enabled);
}

void WindowsWolAdapterControl::log(const QString& message)
{
	wchar_t programData[MAX_PATH] = {};
	if (GetEnvironmentVariableW(L"ProgramData", programData, MAX_PATH) == 0)
	{
		wcsncpy(programData, L"C:\\ProgramData", MAX_PATH - 1);
	}

	const QString directory = QString::fromWCharArray(programData) + QStringLiteral("/Veyon");
	QDir().mkpath(directory);

	PSECURITY_DESCRIPTOR securityDescriptor = nullptr;
	if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
			L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;AU)",
			SDDL_REVISION_1, &securityDescriptor, nullptr))
	{
		const QString nativeDirectory = QDir::toNativeSeparators(directory);
		SetFileSecurityW(reinterpret_cast<LPCWSTR>(nativeDirectory.utf16()),
						 DACL_SECURITY_INFORMATION, securityDescriptor);
		LocalFree(securityDescriptor);
	}

	QFile file(directory + QStringLiteral("/wol-adapter.log"));
	if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
	{
		const auto line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss ")) +
						  message + QLatin1Char('\n');
		file.write(line.toUtf8());
	}
}

void WindowsWolAdapterControl::dumpAdapters(const QString& reason)
{
	logAdapters(reason);
}
