/*
 * PersistentWebFilterState.cpp - HKLM sibling of LocalStore for web filter
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <algorithm>
#include <memory>

#include <QFile>
#include <QSettings>
#include <QUuid>

#include "PersistentWebFilterState.h"
#include "VeyonCore.h"
#include "WebFilterLists.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif


static const auto ModeKey = QStringLiteral("Mode");
static const auto DomainsKey = QStringLiteral("Domains");
static const auto SnapshotKey = QStringLiteral("PolicySnapshot");
static const auto SessionKey = QStringLiteral("Session");
static const auto EmergencyUnlockedKey = QStringLiteral("EmergencyUnlockedSession");
static const auto StateFileEnvVar = QByteArrayLiteral("VEYON_WEBFILTER_STATE_FILE");

#ifdef Q_OS_WIN
static constexpr wchar_t RegistryKey[] = L"SOFTWARE\\Veyon Solutions\\VeyonWebFilter";
static constexpr wchar_t ModeValue[] = L"Mode";
static constexpr wchar_t DomainsValue[] = L"Domains";
static constexpr wchar_t SnapshotValue[] = L"PolicySnapshot";
static constexpr wchar_t SessionValue[] = L"Session";
static constexpr wchar_t EmergencyUnlockedValue[] = L"EmergencyUnlockedSession";
#endif


static QString modeToString(PersistentWebFilterState::Mode mode)
{
	switch (mode)
	{
	case PersistentWebFilterState::Mode::Blacklist:
		return QStringLiteral("Blacklist");
	case PersistentWebFilterState::Mode::Whitelist:
		return QStringLiteral("Whitelist");
	case PersistentWebFilterState::Mode::Off:
		break;
	}
	return QStringLiteral("Off");
}


static PersistentWebFilterState::Mode modeFromString(const QString& text)
{
	if (text.compare(QLatin1String("Blacklist"), Qt::CaseInsensitive) == 0)
	{
		return PersistentWebFilterState::Mode::Blacklist;
	}
	if (text.compare(QLatin1String("Whitelist"), Qt::CaseInsensitive) == 0)
	{
		return PersistentWebFilterState::Mode::Whitelist;
	}
	return PersistentWebFilterState::Mode::Off;
}


static QString testStateFilePath()
{
	return qEnvironmentVariable(StateFileEnvVar.constData());
}


static std::unique_ptr<QSettings> testSettings()
{
	auto settings = std::make_unique<QSettings>(testStateFilePath(), QSettings::IniFormat);
	settings->setFallbacksEnabled(false);
	return settings;
}


#ifdef Q_OS_WIN
static QString readRegistryString(const wchar_t* valueName)
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, RegistryKey, 0,
					  KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
	{
		return {};
	}

	DWORD type = 0;
	DWORD bufferSize = 0;
	if (RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &bufferSize) != ERROR_SUCCESS ||
		type != REG_SZ || bufferSize < sizeof(wchar_t))
	{
		RegCloseKey(key);
		return {};
	}

	QByteArray buffer(int(bufferSize), 0);
	if (RegQueryValueExW(key, valueName, nullptr, &type,
						 reinterpret_cast<LPBYTE>(buffer.data()), &bufferSize) != ERROR_SUCCESS)
	{
		RegCloseKey(key);
		return {};
	}
	RegCloseKey(key);

	auto text = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(buffer.constData()),
										int(bufferSize / sizeof(wchar_t)));
	if (text.endsWith(QLatin1Char('\0')))
	{
		text.chop(1);
	}
	return text;
}


static bool writeRegistryString(const wchar_t* valueName, const QString& text)
{
	HKEY key = nullptr;
	DWORD disposition = 0;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, RegistryKey, 0, nullptr,
						REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY,
						nullptr, &key, &disposition) != ERROR_SUCCESS)
	{
		vWarning() << "failed to open HKLM web-filter key";
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

	if (status == ERROR_SUCCESS)
	{
		RegFlushKey(key);
	}
	RegCloseKey(key);
	if (status != ERROR_SUCCESS)
	{
		vWarning() << "failed to update HKLM web-filter value";
		return false;
	}
	return true;
}
#endif


PersistentWebFilterState::Mode PersistentWebFilterState::readMode()
{
	if (testStateFilePath().isEmpty() == false)
	{
		return modeFromString(testSettings()->value(ModeKey).toString());
	}

#ifdef Q_OS_WIN
	return modeFromString(readRegistryString(ModeValue));
#else
	return Mode::Off;
#endif
}


bool PersistentWebFilterState::writeMode(Mode mode)
{
	if (testStateFilePath().isEmpty() == false)
	{
		if (mode == Mode::Off)
		{
			auto settings = testSettings();
			settings->remove(ModeKey);
			settings->sync();
			return settings->status() == QSettings::NoError;
		}

		auto settings = testSettings();
		settings->setValue(ModeKey, modeToString(mode));
		settings->sync();
		return settings->status() == QSettings::NoError;
	}

#ifdef Q_OS_WIN
	return writeRegistryString(ModeValue, mode == Mode::Off ? QString() : modeToString(mode));
#else
	Q_UNUSED(mode)
	return false;
#endif
}


QStringList PersistentWebFilterState::readDomains()
{
	QString text;
	if (testStateFilePath().isEmpty() == false)
	{
		text = testSettings()->value(DomainsKey).toString();
	}
#ifdef Q_OS_WIN
	else
	{
		text = readRegistryString(DomainsValue);
	}
#endif
	return WebFilterLists::normalizeDomains(text.split(QLatin1Char('\n'), Qt::SkipEmptyParts));
}


bool PersistentWebFilterState::writeDomains(const QStringList& domains)
{
	const auto text = WebFilterLists::normalizeDomains(domains).join(QLatin1Char('\n'));
	if (testStateFilePath().isEmpty() == false)
	{
		auto settings = testSettings();
		if (text.isEmpty())
		{
			settings->remove(DomainsKey);
		}
		else
		{
			settings->setValue(DomainsKey, text);
		}
		settings->sync();
		return settings->status() == QSettings::NoError;
	}

#ifdef Q_OS_WIN
	return writeRegistryString(DomainsValue, text);
#else
	Q_UNUSED(domains)
	return false;
#endif
}


QString PersistentWebFilterState::readSnapshot()
{
	if (testStateFilePath().isEmpty() == false)
	{
		return testSettings()->value(SnapshotKey).toString();
	}

#ifdef Q_OS_WIN
	return readRegistryString(SnapshotValue);
#else
	return {};
#endif
}


bool PersistentWebFilterState::writeSnapshot(const QString& snapshot)
{
	if (testStateFilePath().isEmpty() == false)
	{
		auto settings = testSettings();
		if (snapshot.isEmpty())
		{
			settings->remove(SnapshotKey);
		}
		else
		{
			settings->setValue(SnapshotKey, snapshot);
		}
		settings->sync();
		return settings->status() == QSettings::NoError;
	}

#ifdef Q_OS_WIN
	return writeRegistryString(SnapshotValue, snapshot);
#else
	Q_UNUSED(snapshot)
	return false;
#endif
}


static QString readSessionText()
{
	if (testStateFilePath().isEmpty() == false)
	{
		return testSettings()->value(SessionKey).toString();
	}

#ifdef Q_OS_WIN
	return readRegistryString(SessionValue);
#else
	return {};
#endif
}


static bool writeSessionText(const QString& text)
{
	if (testStateFilePath().isEmpty() == false)
	{
		auto settings = testSettings();
		if (text.isEmpty())
		{
			settings->remove(SessionKey);
		}
		else
		{
			settings->setValue(SessionKey, text);
		}
		settings->sync();
		return settings->status() == QSettings::NoError;
	}

#ifdef Q_OS_WIN
	return writeRegistryString(SessionValue, text);
#else
	Q_UNUSED(text)
	return false;
#endif
}


PersistentWebFilterState::Mode PersistentWebFilterState::mode()
{
	return readMode();
}


QStringList PersistentWebFilterState::domains()
{
	return readDomains();
}


QString PersistentWebFilterState::policySnapshot()
{
	return readSnapshot();
}


bool PersistentWebFilterState::setBlacklist(const QStringList& domains)
{
	return writeDomains(domains) && writeMode(Mode::Blacklist);
}


bool PersistentWebFilterState::setWhitelist(const QStringList& domains)
{
	return writeDomains(domains) && writeMode(Mode::Whitelist);
}


bool PersistentWebFilterState::setPolicySnapshot(const QString& snapshot)
{
	return writeSnapshot(snapshot);
}


WebFilterSession PersistentWebFilterState::session()
{
	bool ok = false;
	auto loaded = WebFilterSession::fromJsonText(readSessionText(), &ok);
	if (ok == false)
	{
		loaded.mode = readMode();
		loaded.domains = readDomains();
		return loaded;
	}
	if (loaded.domains.isEmpty())
	{
		loaded.domains = readDomains();
	}
	if (loaded.mode == Mode::Off)
	{
		loaded.mode = readMode();
	}
	return loaded;
}


bool PersistentWebFilterState::saveSession(const WebFilterSession& session)
{
	if (session.isActive() == false)
	{
		return writeSessionText({}) && writeMode(Mode::Off);
	}

	const bool modeOk = session.mode == Mode::Blacklist ?
							setBlacklist(session.domains) :
							setWhitelist(session.domains);
	return modeOk && writeSessionText(session.toJsonText());
}


bool PersistentWebFilterState::updateCheckpoint(qint64 elapsedMs, qint64 wallMs)
{
	auto current = session();
	if (current.isActive() == false)
	{
		return false;
	}
	current.checkpointElapsedMs = std::max<qint64>(0, elapsedMs);
	current.checkpointWallMs = wallMs;
	return writeSessionText(current.toJsonText());
}


static QString readEmergencyUnlockedText()
{
	if (testStateFilePath().isEmpty() == false)
	{
		return testSettings()->value(EmergencyUnlockedKey).toString();
	}

#ifdef Q_OS_WIN
	return readRegistryString(EmergencyUnlockedValue);
#else
	return {};
#endif
}


static bool writeEmergencyUnlockedText(const QString& text)
{
	if (testStateFilePath().isEmpty() == false)
	{
		auto settings = testSettings();
		if (text.isEmpty())
		{
			settings->remove(EmergencyUnlockedKey);
		}
		else
		{
			settings->setValue(EmergencyUnlockedKey, text);
		}
		settings->sync();
		return settings->status() == QSettings::NoError;
	}

#ifdef Q_OS_WIN
	return writeRegistryString(EmergencyUnlockedValue, text);
#else
	Q_UNUSED(text)
	return false;
#endif
}


bool PersistentWebFilterState::noteEmergencyUnlocked(const QUuid& sessionId)
{
	if (sessionId.isNull())
	{
		return false;
	}
	return writeEmergencyUnlockedText(sessionId.toString(QUuid::WithoutBraces));
}


QUuid PersistentWebFilterState::emergencyUnlockedSession()
{
	const QUuid id{readEmergencyUnlockedText()};
	return id;
}


bool PersistentWebFilterState::isEmergencyUnlocked(const QUuid& sessionId)
{
	if (sessionId.isNull())
	{
		return false;
	}
	return emergencyUnlockedSession() == sessionId;
}


bool PersistentWebFilterState::shouldIgnoreApply(const QUuid& incomingSessionId)
{
	return isEmergencyUnlocked(incomingSessionId);
}


bool PersistentWebFilterState::clearEmergencyUnlocked()
{
	return writeEmergencyUnlockedText({});
}


bool PersistentWebFilterState::clear()
{
	if (writeDomains({}) == false || writeMode(Mode::Off) == false ||
		writeSnapshot({}) == false || writeSessionText({}) == false ||
		clearEmergencyUnlocked() == false)
	{
		return false;
	}

	if (testStateFilePath().isEmpty() == false)
	{
		const auto path = testStateFilePath();
		return QFile::exists(path) == false || QFile::remove(path);
	}

	return true;
}
