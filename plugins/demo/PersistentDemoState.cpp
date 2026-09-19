/*
 * PersistentDemoState.cpp - persist teacher demo in HKLM (Windows)
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QFile>
#include <QSettings>
#include <QUuid>

#include "PersistentDemoState.h"
#include "VeyonCore.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include "VeyonDemoRegistry.h"
#endif


static const auto FeatureUidKey = QStringLiteral("FeatureUid");
static const auto DemoServerHostKey = QStringLiteral("DemoServerHost");
static const auto DemoServerPortKey = QStringLiteral("DemoServerPort");
static const auto DemoAccessTokenKey = QStringLiteral("DemoAccessToken");
static const auto ViewportKey = QStringLiteral("Viewport");
static const auto LockInputKey = QStringLiteral("LockInput");
static const auto StateFileEnvVar = QByteArrayLiteral("VEYON_DEMO_STATE_FILE");


static QString testStateFilePath()
{
	return qEnvironmentVariable(StateFileEnvVar.constData());
}



static QString viewportToString(const QRect& viewport)
{
	if (viewport.isNull())
	{
		return {};
	}

	return QStringLiteral("%1,%2,%3,%4")
			.arg(viewport.x())
			.arg(viewport.y())
			.arg(viewport.width())
			.arg(viewport.height());
}



static QRect viewportFromString(const QString& value)
{
	const auto parts = value.split(QLatin1Char(','));
	if (parts.size() != 4)
	{
		return {};
	}

	return QRect(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt());
}



static PersistentDemoState::Snapshot readFromSettings(QSettings& settings)
{
	PersistentDemoState::Snapshot snapshot;
	const Feature::Uid uid{settings.value(FeatureUidKey).toString()};
	if (uid.isNull())
	{
		return {};
	}

	snapshot.featureUid = uid;
	snapshot.demoServerHost = settings.value(DemoServerHostKey).toString();
	snapshot.demoServerPort = settings.value(DemoServerPortKey).toInt();
	snapshot.demoAccessToken = QByteArray::fromHex(settings.value(DemoAccessTokenKey).toByteArray());
	snapshot.viewport = viewportFromString(settings.value(ViewportKey).toString());
	snapshot.lockInput = settings.value(LockInputKey).toBool();
	return snapshot;
}



static void writeToSettings(QSettings& settings, const PersistentDemoState::Snapshot& snapshot)
{
	settings.setValue(FeatureUidKey, snapshot.featureUid.toString(QUuid::WithoutBraces));
	settings.setValue(DemoServerHostKey, snapshot.demoServerHost);
	settings.setValue(DemoServerPortKey, snapshot.demoServerPort);
	settings.setValue(DemoAccessTokenKey, QString::fromLatin1(snapshot.demoAccessToken.toHex()));
	settings.setValue(ViewportKey, viewportToString(snapshot.viewport));
	settings.setValue(LockInputKey, snapshot.lockInput);
}



static PersistentDemoState::Snapshot readFromTestStateFile()
{
	const auto path = testStateFilePath();
	if (path.isEmpty() || QFile::exists(path) == false)
	{
		return {};
	}

	QSettings settings(path, QSettings::IniFormat);
	settings.setFallbacksEnabled(false);
	return readFromSettings(settings);
}



static bool writeToTestStateFile(const PersistentDemoState::Snapshot& snapshot)
{
	const auto path = testStateFilePath();
	if (path.isEmpty())
	{
		return false;
	}

	if (snapshot.isValid() == false)
	{
		return QFile::exists(path) == false || QFile::remove(path);
	}

	QSettings settings(path, QSettings::IniFormat);
	settings.setFallbacksEnabled(false);
	writeToSettings(settings, snapshot);
	settings.sync();
	return settings.status() == QSettings::NoError;
}



#ifdef Q_OS_WIN
static QString readRegistryString(HKEY key, const wchar_t* valueName)
{
	wchar_t buffer[512];
	DWORD bufferSize = sizeof(buffer);
	DWORD type = 0;
	const auto status = RegQueryValueExW(key, valueName, nullptr, &type,
										 reinterpret_cast<LPBYTE>(buffer), &bufferSize);
	if (status != ERROR_SUCCESS || type != REG_SZ || bufferSize < sizeof(wchar_t))
	{
		return {};
	}

	const auto chars = int(bufferSize / sizeof(wchar_t));
	QString value = QString::fromWCharArray(buffer, chars);
	if (value.endsWith(QLatin1Char('\0')))
	{
		value.chop(1);
	}

	return value;
}



static bool writeRegistryString(HKEY key, const wchar_t* valueName, const QString& value)
{
	const auto wide = value.toStdWString();
	const auto status = RegSetValueExW(key, valueName, 0, REG_SZ,
									   reinterpret_cast<const BYTE *>(wide.c_str()),
									   DWORD((wide.size() + 1) * sizeof(wchar_t)));
	return status == ERROR_SUCCESS;
}



static PersistentDemoState::Snapshot readFromRegistry()
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, VeyonDemoRegistryKey, 0,
					  KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
	{
		return {};
	}

	PersistentDemoState::Snapshot snapshot;
	const Feature::Uid uid{readRegistryString(key, VeyonDemoFeatureUidValue)};
	if (uid.isNull())
	{
		RegCloseKey(key);
		return {};
	}

	snapshot.featureUid = uid;
	snapshot.demoServerHost = readRegistryString(key, VeyonDemoServerHostValue);
	snapshot.demoServerPort = readRegistryString(key, VeyonDemoServerPortValue).toInt();
	snapshot.demoAccessToken = QByteArray::fromHex(readRegistryString(key, VeyonDemoAccessTokenValue).toLatin1());
	snapshot.viewport = viewportFromString(readRegistryString(key, VeyonDemoViewportValue));
	snapshot.lockInput = readRegistryString(key, VeyonDemoLockInputValue) == QLatin1String("1");
	RegCloseKey(key);
	return snapshot;
}



static bool writeToRegistry(const PersistentDemoState::Snapshot& snapshot)
{
	HKEY key = nullptr;
	DWORD disposition = 0;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, VeyonDemoRegistryKey, 0, nullptr,
						REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY,
						nullptr, &key, &disposition) != ERROR_SUCCESS)
	{
		vWarning() << "failed to open HKLM demo key";
		return false;
	}

	if (snapshot.isValid() == false)
	{
		RegDeleteValueW(key, VeyonDemoFeatureUidValue);
		RegDeleteValueW(key, VeyonDemoServerHostValue);
		RegDeleteValueW(key, VeyonDemoServerPortValue);
		RegDeleteValueW(key, VeyonDemoAccessTokenValue);
		RegDeleteValueW(key, VeyonDemoViewportValue);
		RegDeleteValueW(key, VeyonDemoLockInputValue);
		RegCloseKey(key);
		return true;
	}

	const bool ok =
		writeRegistryString(key, VeyonDemoFeatureUidValue, snapshot.featureUid.toString(QUuid::WithoutBraces)) &&
		writeRegistryString(key, VeyonDemoServerHostValue, snapshot.demoServerHost) &&
		writeRegistryString(key, VeyonDemoServerPortValue, QString::number(snapshot.demoServerPort)) &&
		writeRegistryString(key, VeyonDemoAccessTokenValue, QString::fromLatin1(snapshot.demoAccessToken.toHex())) &&
		writeRegistryString(key, VeyonDemoViewportValue, viewportToString(snapshot.viewport)) &&
		writeRegistryString(key, VeyonDemoLockInputValue, snapshot.lockInput ? QStringLiteral("1") : QStringLiteral("0"));

	RegCloseKey(key);

	if (ok == false)
	{
		vWarning() << "failed to update HKLM demo values";
	}

	return ok;
}
#endif



PersistentDemoState::Snapshot PersistentDemoState::readSnapshot()
{
	if (testStateFilePath().isEmpty() == false)
	{
		return readFromTestStateFile();
	}

#ifdef Q_OS_WIN
	return readFromRegistry();
#else
	return {};
#endif
}



bool PersistentDemoState::writeSnapshot(const Snapshot& snapshot)
{
	if (testStateFilePath().isEmpty() == false)
	{
		return writeToTestStateFile(snapshot);
	}

#ifdef Q_OS_WIN
	return writeToRegistry(snapshot);
#else
	Q_UNUSED(snapshot)
	return false;
#endif
}



PersistentDemoState::Snapshot PersistentDemoState::snapshot()
{
	return readSnapshot();
}



bool PersistentDemoState::isActive()
{
	return snapshot().isValid();
}



bool PersistentDemoState::lockInput()
{
	return snapshot().lockInput;
}



bool PersistentDemoState::setActive(const Snapshot& snapshot)
{
	if (snapshot.isValid() == false)
	{
		return clear();
	}

	if (writeSnapshot(snapshot) == false)
	{
		return false;
	}

	vInfo() << "persisted demo" << snapshot.featureUid << snapshot.demoServerHost << snapshot.demoServerPort;
	return true;
}



bool PersistentDemoState::clear()
{
	if (writeSnapshot({}) == false)
	{
		return false;
	}

	vInfo() << "cleared persisted demo";
	return true;
}
