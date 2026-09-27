/*
 * WindowsWebFilterEngine.cpp - hosts file + Chrome/Edge policy web filter
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <winsock2.h>
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include "Filesystem.h"
#include "PersistentWebFilterState.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"
#include "WebFilterConfiguration.h"
#include "WebFilterEngine.h"
#include "WebFilterLists.h"
#include "WebFilterSessionPolicy.h"

namespace
{

struct BrowserPolicy
{
	const wchar_t* blocklist;
	const wchar_t* allowlist;
	const wchar_t* allowlistLegacy;
	const wchar_t* root;
};

constexpr BrowserPolicy BrowserPolicies[] = {
	{ L"SOFTWARE\\Policies\\Google\\Chrome\\URLBlocklist",
	  L"SOFTWARE\\Policies\\Google\\Chrome\\URLAllowlist",
	  L"SOFTWARE\\Policies\\Google\\Chrome\\URLWhitelist",
	  L"SOFTWARE\\Policies\\Google\\Chrome" },
	{ L"SOFTWARE\\Policies\\Microsoft\\Edge\\URLBlocklist",
	  L"SOFTWARE\\Policies\\Microsoft\\Edge\\URLAllowlist",
	  L"SOFTWARE\\Policies\\Microsoft\\Edge\\URLWhitelist",
	  L"SOFTWARE\\Policies\\Microsoft\\Edge" },
	{ L"SOFTWARE\\Policies\\BraveSoftware\\Brave\\URLBlocklist",
	  L"SOFTWARE\\Policies\\BraveSoftware\\Brave\\URLAllowlist",
	  L"SOFTWARE\\Policies\\BraveSoftware\\Brave\\URLWhitelist",
	  L"SOFTWARE\\Policies\\BraveSoftware\\Brave" },
	{ L"SOFTWARE\\Policies\\Chromium\\URLBlocklist",
	  L"SOFTWARE\\Policies\\Chromium\\URLAllowlist",
	  L"SOFTWARE\\Policies\\Chromium\\URLWhitelist",
	  L"SOFTWARE\\Policies\\Chromium" },
	{ L"SOFTWARE\\Policies\\Vivaldi\\URLBlocklist",
	  L"SOFTWARE\\Policies\\Vivaldi\\URLAllowlist",
	  L"SOFTWARE\\Policies\\Vivaldi\\URLWhitelist",
	  L"SOFTWARE\\Policies\\Vivaldi" },
};

constexpr wchar_t FirefoxBlockKey[] = L"SOFTWARE\\Policies\\Mozilla\\Firefox\\WebsiteFilter\\Block";
constexpr wchar_t FirefoxAllowKey[] = L"SOFTWARE\\Policies\\Mozilla\\Firefox\\WebsiteFilter\\Exceptions";
constexpr wchar_t FirefoxDohKey[] = L"SOFTWARE\\Policies\\Mozilla\\Firefox\\DNSOverHTTPS";
constexpr wchar_t InternetSettingsPolicyKey[] = L"SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
constexpr wchar_t InternetSettingsKey[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
constexpr wchar_t AutoConfigValue[] = L"AutoConfigURL";
constexpr wchar_t ProxySettingsPerUserValue[] = L"ProxySettingsPerUser";
constexpr wchar_t DnsOverHttpsMode[] = L"DnsOverHttpsMode";

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

bool writePolicyDword(const wchar_t* keyPath, const wchar_t* valueName, DWORD value)
{
	HKEY key = nullptr;
	DWORD disposition = 0;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, nullptr,
						REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY,
						nullptr, &key, &disposition) != ERROR_SUCCESS)
	{
		return false;
	}
	const auto status = RegSetValueExW(key, valueName, 0, REG_DWORD,
									   reinterpret_cast<const BYTE*>(&value), sizeof(value));
	RegCloseKey(key);
	return status == ERROR_SUCCESS;
}

bool writePolicyString(const wchar_t* keyPath, const wchar_t* valueName, const QString& text)
{
	HKEY key = nullptr;
	DWORD disposition = 0;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, nullptr,
						REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY,
						nullptr, &key, &disposition) != ERROR_SUCCESS)
	{
		return false;
	}

	LONG status = ERROR_SUCCESS;
	if (text.isEmpty())
	{
		status = RegDeleteValueW(key, valueName);
		if (status == ERROR_FILE_NOT_FOUND)
		{
			status = ERROR_SUCCESS;
		}
	}
	else
	{
		const auto wide = text.toStdWString();
		status = RegSetValueExW(key, valueName, 0, REG_SZ,
								reinterpret_cast<const BYTE*>(wide.c_str()),
								DWORD((wide.size() + 1) * sizeof(wchar_t)));
	}
	RegCloseKey(key);
	return status == ERROR_SUCCESS;
}

QString readPolicyString(const wchar_t* keyPath, const wchar_t* valueName)
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0,
					  KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
	{
		return {};
	}

	DWORD type = 0;
	DWORD size = 0;
	if (RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
		type != REG_SZ || size < sizeof(wchar_t))
	{
		RegCloseKey(key);
		return {};
	}

	QByteArray buffer(int(size), 0);
	if (RegQueryValueExW(key, valueName, nullptr, &type,
						 reinterpret_cast<LPBYTE>(buffer.data()), &size) != ERROR_SUCCESS)
	{
		RegCloseKey(key);
		return {};
	}
	RegCloseKey(key);
	auto text = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(buffer.constData()),
										int(size / sizeof(wchar_t)));
	if (text.endsWith(QLatin1Char('\0')))
	{
		text.chop(1);
	}
	return text;
}

QString veyonProgramDataDir()
{
	wchar_t path[MAX_PATH] = {};
	if (SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path) != S_OK)
	{
		return QStringLiteral("C:/ProgramData/Veyon");
	}
	return QDir::fromNativeSeparators(QString::fromWCharArray(path)) + QStringLiteral("/Veyon");
}

QString pacFilePath()
{
	return veyonProgramDataDir() + QStringLiteral("/webfilter.pac");
}

void notifyProxySettingsChanged()
{
	InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
	InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
}

bool removePacFile()
{
	const auto path = pacFilePath();
	return QFile::exists(path) == false || QFile::remove(path);
}

bool deletePolicyValue(const wchar_t* keyPath, const wchar_t* valueName)
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0,
					  KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
	{
		return true;
	}

	const auto status = RegDeleteValueW(key, valueName);
	RegCloseKey(key);
	return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}

bool isVeyonPacUrl(const QString& url)
{
	return url.contains(QLatin1String("webfilter.pac"), Qt::CaseInsensitive);
}

bool leftoverSystemPacPresent()
{
	if (QFile::exists(pacFilePath()))
	{
		return true;
	}

	return isVeyonPacUrl(readPolicyString(InternetSettingsPolicyKey, AutoConfigValue)) ||
		   isVeyonPacUrl(readPolicyString(InternetSettingsKey, AutoConfigValue));
}

bool clearAutoConfigFromKey(HKEY root, const wchar_t* keyPath)
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(root, keyPath, 0,
					  KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
	{
		return true;
	}

	DWORD type = 0;
	DWORD size = 0;
	if (RegQueryValueExW(key, AutoConfigValue, nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
		type != REG_SZ || size < sizeof(wchar_t))
	{
		RegCloseKey(key);
		return true;
	}

	QByteArray buffer(int(size), 0);
	if (RegQueryValueExW(key, AutoConfigValue, nullptr, &type,
						 reinterpret_cast<LPBYTE>(buffer.data()), &size) != ERROR_SUCCESS)
	{
		RegCloseKey(key);
		return true;
	}

	auto text = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(buffer.constData()),
										int(size / sizeof(wchar_t)));
	if (text.endsWith(QLatin1Char('\0')))
	{
		text.chop(1);
	}

	bool ok = true;
	if (isVeyonPacUrl(text))
	{
		const auto status = RegDeleteValueW(key, AutoConfigValue);
		ok = status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
	}
	RegCloseKey(key);
	return ok;
}

void clearAutoConfigFromAllUsers()
{
	HKEY users = nullptr;
	if (RegOpenKeyExW(HKEY_USERS, nullptr, 0, KEY_READ, &users) != ERROR_SUCCESS)
	{
		return;
	}

	for (DWORD index = 0; ; ++index)
	{
		wchar_t name[256] = {};
		DWORD nameLength = 256;
		if (RegEnumKeyExW(users, index, name, &nameLength, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
		{
			break;
		}

		const auto sid = QString::fromWCharArray(name, int(nameLength));
		if (sid.startsWith(QLatin1String("S-")) == false)
		{
			continue;
		}

		const QString keyPath = sid + QStringLiteral("\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Internet Settings");
		clearAutoConfigFromKey(HKEY_USERS, reinterpret_cast<LPCWSTR>(keyPath.utf16()));
	}

	RegCloseKey(users);
}

// Never install a machine-wide PAC. WinINet/AutoConfigURL can send Veyon
// hostnames through PROXY 127.0.0.1:9 and Master then cannot connect.
bool clearSystemPac()
{
	const auto policyUrl = readPolicyString(InternetSettingsPolicyKey, AutoConfigValue);
	const auto userUrl = readPolicyString(InternetSettingsKey, AutoConfigValue);
	const auto ours = leftoverSystemPacPresent();
	bool ok = removePacFile();
	if (isVeyonPacUrl(policyUrl))
	{
		ok = writePolicyString(InternetSettingsPolicyKey, AutoConfigValue, {}) && ok;
	}
	if (isVeyonPacUrl(userUrl))
	{
		ok = writePolicyString(InternetSettingsKey, AutoConfigValue, {}) && ok;
	}
	clearAutoConfigFromAllUsers();
	if (ours)
	{
		ok = deletePolicyValue(InternetSettingsPolicyKey, ProxySettingsPerUserValue) && ok;
	}
	notifyProxySettingsChanged();
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
	QJsonArray browsers;
	for (const auto& browser : BrowserPolicies)
	{
		QJsonObject item;
		item.insert(QStringLiteral("block"), readPolicyValues(browser.blocklist));
		item.insert(QStringLiteral("allow"), readPolicyValues(browser.allowlist));
		item.insert(QStringLiteral("allowLegacy"), readPolicyValues(browser.allowlistLegacy));
		item.insert(QStringLiteral("doh"), readPolicyString(browser.root, DnsOverHttpsMode));
		browsers.append(item);
	}
	root.insert(QStringLiteral("browsers"), browsers);
	root.insert(QStringLiteral("firefoxBlock"), readPolicyValues(FirefoxBlockKey));
	root.insert(QStringLiteral("firefoxAllow"), readPolicyValues(FirefoxAllowKey));
	root.insert(QStringLiteral("autoConfigUrl"), readPolicyString(InternetSettingsPolicyKey, AutoConfigValue));
	root.insert(QStringLiteral("autoConfigUrlUser"), readPolicyString(InternetSettingsKey, AutoConfigValue));
	return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool restorePolicySnapshot(const QString& snapshot)
{
	const auto document = QJsonDocument::fromJson(snapshot.toUtf8());
	const auto root = document.object();
	bool ok = true;
	const auto browsers = root.value(QStringLiteral("browsers")).toArray();
	if (browsers.isEmpty() && root.contains(QStringLiteral("chromeBlock")))
	{
		ok = writePolicyObject(BrowserPolicies[0].blocklist, root.value(QStringLiteral("chromeBlock")).toObject()) && ok;
		ok = writePolicyObject(BrowserPolicies[0].allowlist, root.value(QStringLiteral("chromeAllow")).toObject()) && ok;
		ok = writePolicyObject(BrowserPolicies[1].blocklist, root.value(QStringLiteral("edgeBlock")).toObject()) && ok;
		ok = writePolicyObject(BrowserPolicies[1].allowlist, root.value(QStringLiteral("edgeAllow")).toObject()) && ok;
	}
	else
	{
		for (int i = 0; i < int(sizeof(BrowserPolicies) / sizeof(BrowserPolicies[0])); ++i)
		{
			const auto item = i < browsers.size() ? browsers.at(i).toObject() : QJsonObject();
			ok = writePolicyObject(BrowserPolicies[i].blocklist, item.value(QStringLiteral("block")).toObject()) && ok;
			ok = writePolicyObject(BrowserPolicies[i].allowlist, item.value(QStringLiteral("allow")).toObject()) && ok;
			ok = writePolicyObject(BrowserPolicies[i].allowlistLegacy, item.value(QStringLiteral("allowLegacy")).toObject()) && ok;
			ok = writePolicyString(BrowserPolicies[i].root, DnsOverHttpsMode,
								   item.value(QStringLiteral("doh")).toString()) && ok;
		}
	}
	ok = writePolicyObject(FirefoxBlockKey, root.value(QStringLiteral("firefoxBlock")).toObject()) && ok;
	ok = writePolicyObject(FirefoxAllowKey, root.value(QStringLiteral("firefoxAllow")).toObject()) && ok;
	ok = writePolicyString(InternetSettingsPolicyKey, AutoConfigValue,
						   root.value(QStringLiteral("autoConfigUrl")).toString()) && ok;
	ok = writePolicyString(InternetSettingsKey, AutoConfigValue,
						   root.value(QStringLiteral("autoConfigUrlUser")).toString()) && ok;
	return ok;
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
	for (const auto& image : {
		QStringLiteral("chrome.exe"),
		QStringLiteral("msedge.exe"),
		QStringLiteral("firefox.exe"),
		QStringLiteral("brave.exe"),
		QStringLiteral("opera.exe"),
		QStringLiteral("vivaldi.exe"),
		QStringLiteral("iexplore.exe")
	})
	{
		runHiddenCommand(QStringLiteral("taskkill /IM %1 /F").arg(image));
	}
	notifyProxySettingsChanged();
}

bool applyChromiumLists(const QStringList& block, const QStringList& allow)
{
	bool ok = true;
	for (const auto& browser : BrowserPolicies)
	{
		ok = writePolicyValues(browser.blocklist, block) && ok;
		ok = writePolicyValues(browser.allowlist, allow) && ok;
		ok = writePolicyValues(browser.allowlistLegacy, allow) && ok;
		ok = writePolicyString(browser.root, DnsOverHttpsMode, QStringLiteral("off")) && ok;
	}
	return ok;
}

bool applyFirefoxLists(const QStringList& block, const QStringList& allow)
{
	return writePolicyValues(FirefoxBlockKey, block) &&
		   writePolicyValues(FirefoxAllowKey, allow) &&
		   writePolicyDword(FirefoxDohKey, L"Enabled", 0);
}

bool applyBrowserBlacklist(const QStringList& domains)
{
	const auto patterns = WebFilterLists::chromePolicyPatterns(domains);
	const auto firefox = WebFilterLists::firefoxMatchPatterns(domains);
	return applyChromiumLists(patterns, {}) &&
		   applyFirefoxLists(firefox, {});
}

bool applyBrowserWhitelist(const QStringList& domains)
{
	const auto allow = WebFilterLists::browserAllowPatterns(domains);
	const auto firefoxAllow = WebFilterLists::firefoxMatchPatterns(domains);
	const QStringList block{QStringLiteral("*")};
	const QStringList firefoxBlock{QStringLiteral("<all_urls>")};
	return applyChromiumLists(block, allow) &&
		   applyFirefoxLists(firefoxBlock, firefoxAllow);
}

bool restoreAll(bool restartBrowsers)
{
	const auto snapshot = PersistentWebFilterState::policySnapshot();
	const auto hostsOk = updateHosts({});
	const auto pacOk = clearSystemPac();
	bool policyOk = true;
	if (snapshot.isEmpty())
	{
		for (const auto& browser : BrowserPolicies)
		{
			policyOk = deletePolicyValues(browser.blocklist) && policyOk;
			policyOk = deletePolicyValues(browser.allowlist) && policyOk;
			policyOk = deletePolicyValues(browser.allowlistLegacy) && policyOk;
			policyOk = writePolicyString(browser.root, DnsOverHttpsMode, {}) && policyOk;
		}
		policyOk = deletePolicyValues(FirefoxBlockKey) && policyOk;
		policyOk = deletePolicyValues(FirefoxAllowKey) && policyOk;
		policyOk = writePolicyString(InternetSettingsPolicyKey, AutoConfigValue, {}) && policyOk;
		policyOk = writePolicyString(InternetSettingsKey, AutoConfigValue, {}) && policyOk;
	}
	else
	{
		policyOk = restorePolicySnapshot(snapshot);
	}
	PersistentWebFilterState::clear();
	if (restartBrowsers)
	{
		reloadBrowsers();
	}
	return hostsOk && pacOk && policyOk;
}

}

static WebFilterSession persistableSession(WebFilterSession::Mode mode,
										  const QStringList& domains,
										  const WebFilterSession& incoming)
{
	auto session = incoming;
	if (session.isActive() == false)
	{
		session = WebFilterSessionPolicy::create(mode,
												 WebFilterEngine::configuredMaxTtlMs(),
												 QDateTime::currentMSecsSinceEpoch(),
												 WebFilterEngine::configuredMaxTtlMs(),
												 domains);
	}
	session.mode = mode;
	session.domains = domains;
	if (session.checkpointWallMs <= 0)
	{
		session.checkpointWallMs = session.startTimeMs;
	}
	return session;
}

qint64 WebFilterEngine::configuredMaxTtlMs()
{
	WebFilterConfiguration configuration(&VeyonCore::config());
	const auto minutes = configuration.temporaryWebFilterMaxTtlMinutes();
	if (minutes <= 0)
	{
		return WebFilterSessionPolicy::DefaultMaxTtlMs;
	}
	return qint64(minutes) * 60 * 1000;
}

bool WebFilterEngine::applyBlacklist(const QStringList& schoolBlocked, const QStringList& extraProxies,
									bool restartBrowsers, const WebFilterSession& incomingSession)
{
	if (PersistentWebFilterState::shouldIgnoreApply(incomingSession.sessionId))
	{
		vInfo() << "ignoring teacher apply for emergency-unlocked session"
				<< incomingSession.sessionId.toString(QUuid::WithoutBraces);
		return true;
	}
	PersistentWebFilterState::clearEmergencyUnlocked();
	const auto domains = WebFilterLists::effectiveBlacklist(schoolBlocked, extraProxies);
	if (ensurePolicySnapshot() == false)
	{
		return false;
	}
	if (updateHosts(domains) == false ||
		applyBrowserBlacklist(domains) == false ||
		clearSystemPac() == false)
	{
		return false;
	}
	const auto session = persistableSession(WebFilterSession::Mode::Blacklist, domains, incomingSession);
	PersistentWebFilterState::saveSession(session);
	if (restartBrowsers)
	{
		reloadBrowsers();
	}
	vInfo() << "Web filter session started"
			<< "session=" << session.sessionId.toString(QUuid::WithoutBraces)
			<< "mode=" << WebFilterSessionPolicy::modeName(session.mode)
			<< "duration=" << session.durationMs;
	return true;
}

bool WebFilterEngine::applyWhitelist(const QStringList& schoolAllowed, const QStringList& extraProxies,
									bool restartBrowsers, const WebFilterSession& incomingSession)
{
	if (PersistentWebFilterState::shouldIgnoreApply(incomingSession.sessionId))
	{
		vInfo() << "ignoring teacher apply for emergency-unlocked session"
				<< incomingSession.sessionId.toString(QUuid::WithoutBraces);
		return true;
	}
	PersistentWebFilterState::clearEmergencyUnlocked();
	const auto allowed = WebFilterLists::effectiveAllowlist(schoolAllowed, extraProxies);
	if (ensurePolicySnapshot() == false)
	{
		return false;
	}
	const auto proxyHosts = WebFilterLists::hardcodedProxyDomains() +
							WebFilterLists::hardcodedDohDomains() +
							WebFilterLists::normalizeDomains(extraProxies);
	if (updateHosts(proxyHosts) == false ||
		applyBrowserWhitelist(allowed) == false ||
		clearSystemPac() == false)
	{
		return false;
	}
	const auto session = persistableSession(WebFilterSession::Mode::Whitelist, allowed, incomingSession);
	PersistentWebFilterState::saveSession(session);
	if (restartBrowsers)
	{
		reloadBrowsers();
	}
	vInfo() << "Web filter session started"
			<< "session=" << session.sessionId.toString(QUuid::WithoutBraces)
			<< "mode=" << WebFilterSessionPolicy::modeName(session.mode)
			<< "duration=" << session.durationMs;
	return true;
}

bool WebFilterEngine::restore(bool restartBrowsers)
{
	vInfo() << "restoring web filter";
	return restoreAll(restartBrowsers);
}

bool WebFilterEngine::stopSession(const QUuid& sessionId,
								  WebFilterSession::StopReason reason,
								  bool restartBrowsers)
{
	const auto current = PersistentWebFilterState::session();
	if (WebFilterSessionPolicy::shouldApplyUnlock(sessionId, current.sessionId) == false)
	{
		vInfo() << "Stale unlock ignored"
				<< "incomingSession=" << sessionId.toString(QUuid::WithoutBraces)
				<< "currentSession=" << current.sessionId.toString(QUuid::WithoutBraces);
		return true;
	}

	if (current.isActive() == false &&
		PersistentWebFilterState::mode() == PersistentWebFilterState::Mode::Off &&
		PersistentWebFilterState::policySnapshot().isEmpty())
	{
		vInfo() << "Restore already inactive session"
				<< "session=" << sessionId.toString(QUuid::WithoutBraces)
				<< "reason=" << WebFilterSessionPolicy::reasonName(reason);
		const auto unlockedId = current.sessionId.isNull() ? sessionId : current.sessionId;
		const auto existingUnlocked = PersistentWebFilterState::emergencyUnlockedSession();
		const auto restored = restore(restartBrowsers);
		if (reason == WebFilterSession::StopReason::EmergencyUnlock)
		{
			const auto keep = unlockedId.isNull() == false ? unlockedId : existingUnlocked;
			if (keep.isNull() == false)
			{
				PersistentWebFilterState::noteEmergencyUnlocked(keep);
			}
		}
		return restored;
	}

	if (reason == WebFilterSession::StopReason::HardTTLExpired)
	{
		vWarning() << "Hard TTL exceeded; restoring web"
				   << "session=" << current.sessionId.toString(QUuid::WithoutBraces);
	}
	else if (reason == WebFilterSession::StopReason::ClientTimerExpired)
	{
		vInfo() << "Web filter expired locally"
				<< "session=" << current.sessionId.toString(QUuid::WithoutBraces);
	}
	else if (reason == WebFilterSession::StopReason::TeacherManual)
	{
		vInfo() << "Teacher restore received"
				<< "session=" << current.sessionId.toString(QUuid::WithoutBraces);
	}
	else if (reason == WebFilterSession::StopReason::EmergencyUnlock)
	{
		vInfo() << "Emergency unlock successful"
				<< "session=" << current.sessionId.toString(QUuid::WithoutBraces);
	}
	else
	{
		vInfo() << "stopping web filter session"
				<< "session=" << current.sessionId.toString(QUuid::WithoutBraces)
				<< "reason=" << WebFilterSessionPolicy::reasonName(reason);
	}

	const auto unlockedId = current.sessionId.isNull() ? sessionId : current.sessionId;
	const auto restored = restore(restartBrowsers);
	if (reason == WebFilterSession::StopReason::EmergencyUnlock && unlockedId.isNull() == false)
	{
		PersistentWebFilterState::noteEmergencyUnlocked(unlockedId);
	}
	return restored;
}

bool WebFilterEngine::reconcileOnServiceStart()
{
	ensureClassroomFirewall();
	clearSystemPac();

	const auto loaded = PersistentWebFilterState::session();
	if (PersistentWebFilterState::shouldIgnoreApply(loaded.sessionId))
	{
		vInfo() << "not reapplying emergency-unlocked session after reboot"
				<< loaded.sessionId.toString(QUuid::WithoutBraces);
		return restore(false) && PersistentWebFilterState::noteEmergencyUnlocked(loaded.sessionId);
	}
	QString reconstructReason;
	const auto session = WebFilterSessionPolicy::recoverForReboot(
				loaded, QDateTime::currentMSecsSinceEpoch(), configuredMaxTtlMs(), &reconstructReason);
	if (reconstructReason.contains(QLatin1String("reconstructed")))
	{
		vWarning() << "persisted web-filter session JSON missing; reconstructing from mode"
				   << reconstructReason;
	}
	if (session.isActive() || PersistentWebFilterState::mode() != PersistentWebFilterState::Mode::Off)
	{
		QString reason;
		const auto action = WebFilterSessionPolicy::recoveryAction(
					session, QDateTime::currentMSecsSinceEpoch(), configuredMaxTtlMs(), &reason);
		if (action == WebFilterSessionPolicy::RecoveryAction::FailOpen)
		{
			vWarning() << "Malformed persisted web filter state; clearing" << reason;
			return restore(false);
		}
		if (action == WebFilterSessionPolicy::RecoveryAction::Expire)
		{
			vInfo() << "Web filter expired on recovery"
					<< "session=" << session.sessionId.toString(QUuid::WithoutBraces)
					<< reason;
			return stopSession(session.sessionId, WebFilterSession::StopReason::RecoveryExpired, false);
		}

		const auto elapsed = WebFilterSessionPolicy::recoveredElapsedMs(
					session, QDateTime::currentMSecsSinceEpoch(), configuredMaxTtlMs());
		auto resumed = session;
		resumed.checkpointElapsedMs = elapsed;
		resumed.checkpointWallMs = QDateTime::currentMSecsSinceEpoch();
		vInfo() << "Web filter restored after reboot"
				<< "session=" << resumed.sessionId.toString(QUuid::WithoutBraces)
				<< "mode=" << WebFilterSessionPolicy::modeName(resumed.mode);
		if (resumed.mode == WebFilterSession::Mode::Blacklist)
		{
			return applyBlacklist(resumed.domains, {}, false, resumed);
		}
		return applyWhitelist(resumed.domains, {}, false, resumed);
	}

	if (PersistentWebFilterState::policySnapshot().isEmpty() == false ||
		WebFilterLists::hostsSectionPresent(readHostsFile()))
	{
		vInfo() << "clearing leftover web filter artifacts";
		return restore(false);
	}

	if (leftoverSystemPacPresent())
	{
		vInfo() << "clearing leftover system PAC so Veyon can connect";
		return clearSystemPac();
	}

	return true;
}

void WebFilterEngine::ensureClassroomFirewall()
{
	if (VeyonCore::config().isFirewallExceptionEnabled() == false)
	{
		return;
	}

	const auto port = VeyonCore::config().veyonServerPort();
	const auto server = QDir::toNativeSeparators(VeyonCore::filesystem().serverFilePath());
	const auto worker = QDir::toNativeSeparators(VeyonCore::filesystem().workerFilePath());

	const auto replaceRule = [](const QString& name, const QString& spec) {
		runHiddenCommand(QStringLiteral("netsh advfirewall firewall delete rule name=\"%1\"").arg(name));
		runHiddenCommand(QStringLiteral("netsh advfirewall firewall add rule name=\"%1\" %2").arg(name, spec));
	};

	// Port + ICMP rules survive "block all incoming apps" better than an
	// application rule alone. Do not use the remove-all-then-add helper that
	// can leave TCP 11100 closed if COM add fails.
	replaceRule(QStringLiteral("CYC Veyon Server Port"),
				QStringLiteral("dir=in action=allow protocol=TCP localport=%1 profile=any enable=yes").arg(port));
	replaceRule(QStringLiteral("CYC Veyon ICMP Echo"),
				QStringLiteral("dir=in action=allow protocol=icmpv4:8,any profile=any enable=yes"));
	if (server.isEmpty() == false)
	{
		replaceRule(QStringLiteral("CYC Veyon Server App"),
					QStringLiteral("dir=in action=allow program=\"%1\" protocol=TCP profile=any enable=yes").arg(server));
	}
	if (worker.isEmpty() == false)
	{
		replaceRule(QStringLiteral("CYC Veyon Worker App"),
					QStringLiteral("dir=in action=allow program=\"%1\" protocol=TCP profile=any enable=yes").arg(worker));
	}

	// blockinboundalways ignores allow rules. Restore normal exception policy.
	runHiddenCommand(QStringLiteral("netsh advfirewall set allprofiles firewallpolicy blockinbound,allowoutbound"));
	vInfo() << "ensured classroom firewall rules for port" << port;
}
