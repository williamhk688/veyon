/*
 * PersistentScreenLockState.cpp - persist teacher screen-lock in HKLM (Windows)
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <QFile>
#include <QSettings>

#include "PersistentScreenLockState.h"
#include "VeyonCore.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include "VeyonScreenLockRegistry.h"
#endif


static const auto LockedFeatureUidKey = QStringLiteral("LockedFeatureUid");
static const auto StateFileEnvVar = QByteArrayLiteral("VEYON_SCREENLOCK_STATE_FILE");


static QString testStateFilePath()
{
	return qEnvironmentVariable(StateFileEnvVar.constData());
}



static Feature::Uid readFromTestStateFile()
{
	const auto path = testStateFilePath();
	if (path.isEmpty() || QFile::exists(path) == false)
	{
		return {};
	}

	QSettings settings(path, QSettings::IniFormat);
	settings.setFallbacksEnabled(false);
	const Feature::Uid uid{settings.value(LockedFeatureUidKey).toString()};
	return uid.isNull() ? Feature::Uid{} : uid;
}



static bool writeToTestStateFile(const Feature::Uid& featureUid)
{
	const auto path = testStateFilePath();
	if (path.isEmpty())
	{
		return false;
	}

	if (featureUid.isNull())
	{
		return QFile::exists(path) == false || QFile::remove(path);
	}

	QSettings settings(path, QSettings::IniFormat);
	settings.setFallbacksEnabled(false);
	settings.setValue(LockedFeatureUidKey, featureUid.toString(QUuid::WithoutBraces));
	settings.sync();
	return settings.status() == QSettings::NoError;
}



#ifdef Q_OS_WIN
static Feature::Uid readFromRegistry()
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, VeyonScreenLockRegistryKey, 0,
					  KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
	{
		return {};
	}

	wchar_t buffer[128];
	DWORD bufferSize = sizeof(buffer);
	DWORD type = 0;
	const auto status = RegQueryValueExW(key, VeyonScreenLockRegistryValue,
										 nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &bufferSize);
	RegCloseKey(key);

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

	const Feature::Uid uid{value};
	return uid.isNull() ? Feature::Uid{} : uid;
}



static bool writeToRegistry(const Feature::Uid& featureUid)
{
	HKEY key = nullptr;
	DWORD disposition = 0;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, VeyonScreenLockRegistryKey, 0, nullptr,
						REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY,
						nullptr, &key, &disposition) != ERROR_SUCCESS)
	{
		vWarning() << "failed to open HKLM screen-lock key";
		return false;
	}

	LONG status = ERROR_SUCCESS;
	if (featureUid.isNull())
	{
		status = RegDeleteValueW(key, VeyonScreenLockRegistryValue);
		if (status == ERROR_FILE_NOT_FOUND)
		{
			status = ERROR_SUCCESS;
		}
	}
	else
	{
		const auto uidString = featureUid.toString(QUuid::WithoutBraces).toStdWString();
		status = RegSetValueExW(key, VeyonScreenLockRegistryValue, 0, REG_SZ,
								reinterpret_cast<const BYTE *>(uidString.c_str()),
								DWORD((uidString.size() + 1) * sizeof(wchar_t)));
	}

	RegCloseKey(key);

	if (status != ERROR_SUCCESS)
	{
		vWarning() << "failed to update HKLM screen-lock value, status" << unsigned(status);
		return false;
	}

	return true;
}
#endif



Feature::Uid PersistentScreenLockState::readLockedFeatureUid()
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



bool PersistentScreenLockState::writeLockedFeatureUid(const Feature::Uid& featureUid)
{
	if (testStateFilePath().isEmpty() == false)
	{
		return writeToTestStateFile(featureUid);
	}

#ifdef Q_OS_WIN
	return writeToRegistry(featureUid);
#else
	Q_UNUSED(featureUid)
	return false;
#endif
}



Feature::Uid PersistentScreenLockState::lockedFeatureUid()
{
	return readLockedFeatureUid();
}



bool PersistentScreenLockState::isLocked()
{
	return lockedFeatureUid().isNull() == false;
}



bool PersistentScreenLockState::setLocked(const Feature::Uid& featureUid)
{
	if (featureUid.isNull())
	{
		return clear();
	}

	if (writeLockedFeatureUid(featureUid) == false)
	{
		return false;
	}

	vInfo() << "persisted screen lock" << featureUid;
	return true;
}



bool PersistentScreenLockState::clear()
{
	if (writeLockedFeatureUid({}) == false)
	{
		return false;
	}

	vInfo() << "cleared persisted screen lock";
	return true;
}
