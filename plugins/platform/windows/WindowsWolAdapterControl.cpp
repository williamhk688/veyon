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
#include <objbase.h>
#include <netcon.h>

#include <QElapsedTimer>
#include <QHostAddress>
#include <QMutex>
#include <QStringList>
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

bool runHiddenCommand(const QString& command)
{
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
		vWarning() << "CreateProcess failed" << GetLastError();
		return false;
	}

	const DWORD waited = WaitForSingleObject(processInfo.hProcess, 8000);
	DWORD exitCode = 1;
	GetExitCodeProcess(processInfo.hProcess, &exitCode);
	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);
	return waited == WAIT_OBJECT_0 && exitCode == 0;
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

	const QString netsh = QStringLiteral("\"%1\\System32\\netsh.exe\"").arg(QString::fromWCharArray(systemRoot));
	const QString admin = enabled ? QStringLiteral("ENABLED") : QStringLiteral("DISABLED");
	const QStringList commands{
		QStringLiteral("%1 interface set interface name=\"%2\" admin=%3").arg(netsh, alias, admin),
		QStringLiteral("%1 interface set interface \"%2\" admin=%3").arg(netsh, alias, admin),
		QStringLiteral("%1 interface set interface name=\"%2\" admin=%3")
			.arg(netsh, alias, enabled ? QStringLiteral("enable") : QStringLiteral("disable"))
	};

	for (const auto& command : commands)
	{
		if (runHiddenCommand(command))
		{
			QThread::msleep(400);
			return true;
		}
	}

	return false;
}

void freeNetconProperties(NETCON_PROPERTIES* properties)
{
	if (properties == nullptr)
	{
		return;
	}

	CoTaskMemFree(properties->pszwName);
	CoTaskMemFree(properties->pszwDeviceName);
	CoTaskMemFree(properties);
}

bool setNetConnectionEnabled(const QString& alias, bool enabled)
{
	if (alias.isEmpty() || enabled == false)
	{
		return false;
	}

	const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const bool uninitialize = initialized == S_OK;

	static const GUID kClsidConnectionManager =
		{0xBA126AD1, 0x2166, 0x11D1, {0xB1, 0xD0, 0x00, 0x80, 0x5F, 0xC1, 0x27, 0x0E}};
	static const GUID kIidINetConnectionManager =
		{0xC08956A2, 0x1CD3, 0x11D1, {0xB1, 0xC5, 0x00, 0x80, 0x5F, 0xC1, 0x27, 0x0E}};

	INetConnectionManager* manager = nullptr;
	HRESULT hr = CoCreateInstance(kClsidConnectionManager, nullptr, CLSCTX_ALL,
								  kIidINetConnectionManager, reinterpret_cast<void**>(&manager));
	if (FAILED(hr) || manager == nullptr)
	{
		vWarning() << "INetConnectionManager failed" << int(hr);
		if (uninitialize)
		{
			CoUninitialize();
		}
		return false;
	}

	IEnumNetConnection* enumerator = nullptr;
	hr = manager->EnumConnections(NCME_DEFAULT, &enumerator);
	bool success = false;
	if (SUCCEEDED(hr) && enumerator)
	{
		INetConnection* connection = nullptr;
		ULONG fetched = 0;
		while (enumerator->Next(1, &connection, &fetched) == S_OK && connection)
		{
			NETCON_PROPERTIES* properties = nullptr;
			if (SUCCEEDED(connection->GetProperties(&properties)) && properties)
			{
				const QString name = QString::fromWCharArray(properties->pszwName);
				if (name.compare(alias, Qt::CaseInsensitive) == 0)
				{
					hr = connection->Connect();
					success = SUCCEEDED(hr);
				}
				freeNetconProperties(properties);
			}
			connection->Release();
			if (success)
			{
				break;
			}
		}
		enumerator->Release();
	}

	manager->Release();
	if (uninitialize)
	{
		CoUninitialize();
	}
	return success;
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
		if (adapter.index == 0)
		{
			vWarning() << "no Ethernet adapter found for Wake-on-LAN";
			return;
		}

		m_index = adapter.index;
		m_originalAdmin = adapter.adminStatus;

		vDebug() << "WOL using interface" << adapter.index << adapter.alias
				 << "admin" << adapter.adminStatus << "oper" << adapter.operStatus
				 << adapter.ipv4.toString();

		if (adapter.adminStatus != NET_IF_ADMIN_STATUS_UP)
		{
			if (WindowsWolAdapterControl::setAdminStatus(adapter.index, true) == false)
			{
				vWarning() << "failed to enable WOL interface" << adapter.index;
				return;
			}
			m_changed = true;
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
		}
		else
		{
			vDebug() << "restored WOL interface" << m_index << "enabled" << enable;
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

	// SetIfEntry can report success without changing ncpa.cpl. Use netsh first.
	if (setNetshAdminStatus(before.alias, enabled))
	{
		return true;
	}

	if (enabled && setNetConnectionEnabled(before.alias, true))
	{
		return true;
	}

	if (setSetupDiAdminStatus(interfaceIndex, enabled))
	{
		return true;
	}

	setIfEntryAdminStatus(interfaceIndex, enabled);
	vWarning() << "native enable/disable failed for" << interfaceIndex << "enabled" << enabled
			   << "lastError" << GetLastError();
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
