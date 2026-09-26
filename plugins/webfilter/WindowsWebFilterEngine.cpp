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

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include "PersistentWebFilterState.h"
#include "VeyonCore.h"
#include "WebFilterEngine.h"
#include "WebFilterLists.h"

namespace
{

struct BrowserPolicy
{
	const wchar_t* blocklist;
	const wchar_t* allowlist;
	const wchar_t* root;
};

constexpr BrowserPolicy BrowserPolicies[] = {
	{ L"SOFTWARE\\Policies\\Google\\Chrome\\URLBlocklist",
	  L"SOFTWARE\\Policies\\Google\\Chrome\\URLAllowlist",
	  L"SOFTWARE\\Policies\\Google\\Chrome" },
	{ L"SOFTWARE\\Policies\\Microsoft\\Edge\\URLBlocklist",
	  L"SOFTWARE\\Policies\\Microsoft\\Edge\\URLAllowlist",
	  L"SOFTWARE\\Policies\\Microsoft\\Edge" },
	{ L"SOFTWARE\\Policies\\BraveSoftware\\Brave\\URLBlocklist",
	  L"SOFTWARE\\Policies\\BraveSoftware\\Brave\\URLAllowlist",
	  L"SOFTWARE\\Policies\\BraveSoftware\\Brave" },
	{ L"SOFTWARE\\Policies\\Chromium\\URLBlocklist",
	  L"SOFTWARE\\Policies\\Chromium\\URLAllowlist",
	  L"SOFTWARE\\Policies\\Chromium" },
	{ L"SOFTWARE\\Policies\\Vivaldi\\URLBlocklist",
	  L"SOFTWARE\\Policies\\Vivaldi\\URLAllowlist",
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

QString pacFileUrl()
{
	return QStringLiteral("file:///") + pacFilePath();
}

void notifyProxySettingsChanged()
{
	InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
	InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
}

bool writePacFile(const QString& script)
{
	const auto dir = veyonProgramDataDir();
	if (QDir().mkpath(dir) == false)
	{
		return false;
	}
	QFile file(pacFilePath());
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate) == false)
	{
		vWarning() << "can't write PAC file" << file.errorString();
		return false;
	}
	const auto utf8 = script.toUtf8();
	return file.write(utf8) == utf8.size();
}

bool removePacFile()
{
	const auto path = pacFilePath();
	return QFile::exists(path) == false || QFile::remove(path);
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

bool applySystemPac(bool whitelistMode, const QStringList& domains, const QStringList& extraProxies)
{
	const auto script = WebFilterLists::proxyPacScript(whitelistMode, domains, extraProxies);
	if (writePacFile(script) == false)
	{
		return false;
	}
	const auto url = pacFileUrl();
	return writePolicyDword(InternetSettingsPolicyKey, ProxySettingsPerUserValue, 0) &&
		   writePolicyString(InternetSettingsPolicyKey, AutoConfigValue, url) &&
		   writePolicyString(InternetSettingsKey, AutoConfigValue, url);
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

bool restoreAll()
{
	const auto snapshot = PersistentWebFilterState::policySnapshot();
	const auto hostsOk = updateHosts({});
	const auto pacOk = removePacFile();
	bool policyOk = true;
	if (snapshot.isEmpty())
	{
		for (const auto& browser : BrowserPolicies)
		{
			policyOk = deletePolicyValues(browser.blocklist) && policyOk;
			policyOk = deletePolicyValues(browser.allowlist) && policyOk;
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
	reloadBrowsers();
	return hostsOk && pacOk && policyOk;
}

}

bool WebFilterEngine::applyBlacklist(const QStringList& schoolBlocked, const QStringList& extraProxies)
{
	const auto domains = WebFilterLists::effectiveBlacklist(schoolBlocked, extraProxies);
	if (ensurePolicySnapshot() == false)
	{
		return false;
	}
	if (updateHosts(domains) == false ||
		applyBrowserBlacklist(domains) == false ||
		applySystemPac(false, schoolBlocked, extraProxies) == false)
	{
		return false;
	}
	PersistentWebFilterState::setBlacklist(domains);
	reloadBrowsers();
	vInfo() << "applied web blacklist" << domains.size() << "domains";
	return true;
}

bool WebFilterEngine::applyWhitelist(const QStringList& schoolAllowed, const QStringList& extraProxies)
{
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
		applySystemPac(true, allowed, extraProxies) == false)
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
