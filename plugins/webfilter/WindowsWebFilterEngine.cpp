/*
 * WindowsWebFilterEngine.cpp - hosts file + Chrome/Edge policy web filter
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <winsock2.h>
#include <windows.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include "PersistentWebFilterState.h"
#include "VeyonCore.h"
#include "WebFilterEngine.h"
#include "WebFilterLists.h"

namespace
{

constexpr wchar_t ChromeBlocklistKey[] = L"SOFTWARE\\Policies\\Google\\Chrome\\URLBlocklist";
constexpr wchar_t ChromeAllowlistKey[] = L"SOFTWARE\\Policies\\Google\\Chrome\\URLAllowlist";
constexpr wchar_t EdgeBlocklistKey[] = L"SOFTWARE\\Policies\\Microsoft\\Edge\\URLBlocklist";
constexpr wchar_t EdgeAllowlistKey[] = L"SOFTWARE\\Policies\\Microsoft\\Edge\\URLAllowlist";

bool runHiddenCommand(const QString& command)
{
	auto commandLineBuffer = command.toStdWString();
	STARTUPINFOW startupInfo{};
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION processInfo{};

	if (CreateProcessW(nullptr, commandLineBuffer.data(), nullptr, nullptr, FALSE,
					   CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo) == FALSE)
	{
		vWarning() << "CreateProcess failed" << GetLastError();
		return false;
	}

	WaitForSingleObject(processInfo.hProcess, 8000);
	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);
	return true;
}

QString hostsFilePath()
{
	wchar_t windowsDirectory[MAX_PATH] = {};
	const auto length = GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
	{
		return QStringLiteral("C:/Windows/System32/drivers/etc/hosts");
	}

	return QDir::fromNativeSeparators(QString::fromWCharArray(windowsDirectory, int(length))) +
		   QStringLiteral("/System32/drivers/etc/hosts");
}

QByteArray readHostsFile()
{
	QFile file(hostsFilePath());
	if (file.exists() == false)
	{
		return {};
	}
	if (file.open(QIODevice::ReadOnly) == false)
	{
		vWarning() << "can't read hosts file" << file.errorString();
		return {};
	}
	return file.readAll();
}

bool writeHostsFile(const QByteArray& contents)
{
	QFile file(hostsFilePath());
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate) == false)
	{
		vWarning() << "can't write hosts file" << file.errorString();
		return false;
	}
	if (file.write(contents) != contents.size())
	{
		vWarning() << "incomplete hosts file write";
		return false;
	}
	return true;
}

bool updateHosts(const QStringList& domains)
{
	const auto updated = domains.isEmpty()
			? WebFilterLists::removeHostsSection(readHostsFile())
			: WebFilterLists::replaceHostsSection(readHostsFile(), domains);
	if (writeHostsFile(updated) == false)
	{
		return false;
	}
	runHiddenCommand(QStringLiteral("ipconfig /flushdns"));
	return true;
}

QJsonObject readPolicyValues(const wchar_t* keyPath)
{
	QJsonObject values;
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0,
					  KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
	{
		return values;
	}

	for (DWORD index = 0; ; ++index)
	{
		wchar_t name[256] = {};
		DWORD nameLength = 256;
		DWORD type = 0;
		DWORD dataSize = 0;
		const auto enumStatus = RegEnumValueW(key, index, name, &nameLength,
											  nullptr, &type, nullptr, &dataSize);
		if (enumStatus == ERROR_NO_MORE_ITEMS)
		{
			break;
		}
		if (enumStatus != ERROR_SUCCESS || type != REG_SZ || dataSize < sizeof(wchar_t))
		{
			continue;
		}

		QByteArray data(int(dataSize), 0);
		nameLength = 256;
		if (RegEnumValueW(key, index, name, &nameLength, nullptr, &type,
						  reinterpret_cast<LPBYTE>(data.data()), &dataSize) != ERROR_SUCCESS)
		{
			continue;
		}

		auto text = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(data.constData()),
											int(dataSize / sizeof(wchar_t)));
		if (text.endsWith(QLatin1Char('\0')))
		{
			text.chop(1);
		}
		values.insert(QString::fromWCharArray(name, int(nameLength)), text);
	}

	RegCloseKey(key);
	return values;
}

bool deletePolicyValues(const wchar_t* keyPath)
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0,
					  KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
	{
		return true;
	}

	QStringList names;
	for (DWORD index = 0; ; ++index)
	{
		wchar_t name[256] = {};
		DWORD nameLength = 256;
		if (RegEnumValueW(key, index, name, &nameLength, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
		{
			break;
		}
		names.append(QString::fromWCharArray(name, int(nameLength)));
	}

	for (const auto& name : names)
	{
		RegDeleteValueW(key, reinterpret_cast<LPCWSTR>(name.utf16()));
	}

	RegCloseKey(key);
	return true;
}

bool writePolicyValues(const wchar_t* keyPath, const QStringList& patterns)
{
	if (deletePolicyValues(keyPath) == false)
	{
		return false;
	}

	if (patterns.isEmpty())
	{
		return true;
	}

	HKEY key = nullptr;
	DWORD disposition = 0;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, nullptr,
						REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY,
						nullptr, &key, &disposition) != ERROR_SUCCESS)
	{
		vWarning() << "can't create browser policy key";
		return false;
	}

	bool ok = true;
	for (int i = 0; i < patterns.size(); ++i)
	{
		const auto valueName = QString::number(i + 1).toStdWString();
		const auto value = patterns.at(i).toStdWString();
		if (RegSetValueExW(key, valueName.c_str(), 0, REG_SZ,
						   reinterpret_cast<const BYTE*>(value.c_str()),
						   DWORD((value.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS)
		{
			ok = false;
			break;
		}
	}

	RegCloseKey(key);
	return ok;
}

bool writePolicyObject(const wchar_t* keyPath, const QJsonObject& values)
{
	if (deletePolicyValues(keyPath) == false)
	{
		return false;
	}

	if (values.isEmpty())
	{
		return true;
	}

	HKEY key = nullptr;
	DWORD disposition = 0;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, nullptr,
						REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY,
						nullptr, &key, &disposition) != ERROR_SUCCESS)
	{
		return false;
	}

	bool ok = true;
	for (auto it = values.begin(); it != values.end(); ++it)
	{
		const auto name = it.key().toStdWString();
		const auto value = it.value().toString().toStdWString();
		if (RegSetValueExW(key, name.c_str(), 0, REG_SZ,
						   reinterpret_cast<const BYTE*>(value.c_str()),
						   DWORD((value.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS)
		{
			ok = false;
			break;
		}
	}

	RegCloseKey(key);
	return ok;
}

QString capturePolicySnapshot()
{
	QJsonObject root;
	root.insert(QStringLiteral("chromeBlock"), readPolicyValues(ChromeBlocklistKey));
	root.insert(QStringLiteral("chromeAllow"), readPolicyValues(ChromeAllowlistKey));
	root.insert(QStringLiteral("edgeBlock"), readPolicyValues(EdgeBlocklistKey));
	root.insert(QStringLiteral("edgeAllow"), readPolicyValues(EdgeAllowlistKey));
	return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool restorePolicySnapshot(const QString& snapshot)
{
	const auto document = QJsonDocument::fromJson(snapshot.toUtf8());
	const auto root = document.object();
	return writePolicyObject(ChromeBlocklistKey, root.value(QStringLiteral("chromeBlock")).toObject()) &&
		   writePolicyObject(ChromeAllowlistKey, root.value(QStringLiteral("chromeAllow")).toObject()) &&
		   writePolicyObject(EdgeBlocklistKey, root.value(QStringLiteral("edgeBlock")).toObject()) &&
		   writePolicyObject(EdgeAllowlistKey, root.value(QStringLiteral("edgeAllow")).toObject());
}

bool ensurePolicySnapshot()
{
	if (PersistentWebFilterState::policySnapshot().isEmpty() == false)
	{
		return true;
	}

	return PersistentWebFilterState::setPolicySnapshot(capturePolicySnapshot());
}

void reloadBrowsers()
{
	runHiddenCommand(QStringLiteral("taskkill /IM chrome.exe /F"));
	runHiddenCommand(QStringLiteral("taskkill /IM msedge.exe /F"));
}

bool applyBrowserBlacklist(const QStringList& domains)
{
	const auto patterns = WebFilterLists::chromeUrlPatterns(domains);
	return writePolicyValues(ChromeBlocklistKey, patterns) &&
		   writePolicyValues(EdgeBlocklistKey, patterns) &&
		   writePolicyValues(ChromeAllowlistKey, {}) &&
		   writePolicyValues(EdgeAllowlistKey, {});
}

bool applyBrowserWhitelist(const QStringList& domains)
{
	const auto allow = WebFilterLists::browserAllowPatterns(domains);
	const QStringList block{QStringLiteral("*")};
	return writePolicyValues(ChromeBlocklistKey, block) &&
		   writePolicyValues(EdgeBlocklistKey, block) &&
		   writePolicyValues(ChromeAllowlistKey, allow) &&
		   writePolicyValues(EdgeAllowlistKey, allow);
}

bool restoreAll()
{
	const auto snapshot = PersistentWebFilterState::policySnapshot();
	const auto hostsOk = updateHosts({});
	const auto policyOk = snapshot.isEmpty()
			? deletePolicyValues(ChromeBlocklistKey) &&
			  deletePolicyValues(ChromeAllowlistKey) &&
			  deletePolicyValues(EdgeBlocklistKey) &&
			  deletePolicyValues(EdgeAllowlistKey)
			: restorePolicySnapshot(snapshot);
	PersistentWebFilterState::clear();
	reloadBrowsers();
	return hostsOk && policyOk;
}

}

bool WebFilterEngine::applyBlacklist(const QStringList& schoolBlocked)
{
	const auto domains = WebFilterLists::effectiveBlacklist(schoolBlocked);
	if (ensurePolicySnapshot() == false)
	{
		return false;
	}
	if (updateHosts(domains) == false || applyBrowserBlacklist(domains) == false)
	{
		return false;
	}
	PersistentWebFilterState::setBlacklist(domains);
	reloadBrowsers();
	vInfo() << "applied web blacklist" << domains.size() << "domains";
	return true;
}

bool WebFilterEngine::applyWhitelist(const QStringList& schoolAllowed)
{
	const auto allowed = WebFilterLists::effectiveAllowlist(schoolAllowed);
	if (ensurePolicySnapshot() == false)
	{
		return false;
	}
	// Keep hardcoded proxies out of hosts-based name resolution too.
	if (updateHosts(WebFilterLists::hardcodedProxyDomains() + WebFilterLists::hardcodedDohDomains()) == false ||
		applyBrowserWhitelist(allowed) == false)
	{
		return false;
	}
	PersistentWebFilterState::setWhitelist(allowed);
	reloadBrowsers();
	vInfo() << "applied web whitelist" << allowed.size() << "domains";
	return true;
}

bool WebFilterEngine::restore()
{
	vInfo() << "restoring web filter";
	return restoreAll();
}

bool WebFilterEngine::reconcileOnServiceStart()
{
	const auto mode = PersistentWebFilterState::mode();
	if (mode == PersistentWebFilterState::Mode::Blacklist)
	{
		const auto domains = PersistentWebFilterState::domains();
		vInfo() << "reapplying persisted web blacklist";
		return applyBlacklist(domains);
	}

	if (mode == PersistentWebFilterState::Mode::Whitelist)
	{
		vInfo() << "dropping web whitelist after service start";
		return restore();
	}

	if (PersistentWebFilterState::policySnapshot().isEmpty() == false ||
		WebFilterLists::hostsSectionPresent(readHostsFile()))
	{
		vInfo() << "clearing leftover web filter artifacts";
		return restore();
	}

	return true;
}
