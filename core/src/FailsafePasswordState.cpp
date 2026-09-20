/*
 * FailsafePasswordState.cpp - failsafe unlock password and lock-clear helpers
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

#include "FailsafePasswordState.h"
#include "Logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include "VeyonDemoRegistry.h"
#include "VeyonFailsafeRegistry.h"
#include "VeyonScreenLockRegistry.h"
#endif


static const auto PasswordKey = QStringLiteral("FailsafePassword");
static const auto PasswordFileEnvVar = QByteArrayLiteral("VEYON_FAILSAFE_PASSWORD_FILE");
static const auto ScreenLockStateFileEnvVar = QByteArrayLiteral("VEYON_SCREENLOCK_STATE_FILE");
static const auto DemoStateFileEnvVar = QByteArrayLiteral("VEYON_DEMO_STATE_FILE");


static QString passwordTestFilePath()
{
	return qEnvironmentVariable(PasswordFileEnvVar.constData());
}



static QString readFromTestStateFile()
{
	const auto path = passwordTestFilePath();
	if (path.isEmpty() || QFile::exists(path) == false)
	{
		return {};
	}

	QSettings settings(path, QSettings::IniFormat);
	settings.setFallbacksEnabled(false);
	return settings.value(PasswordKey).toString();
}



static bool writeToTestStateFile(const QString& password)
{
	const auto path = passwordTestFilePath();
	if (path.isEmpty())
	{
		return false;
	}

	if (password.isEmpty())
	{
		return QFile::exists(path) == false || QFile::remove(path);
	}

	QSettings settings(path, QSettings::IniFormat);
	settings.setFallbacksEnabled(false);
	settings.setValue(PasswordKey, password);
	settings.sync();
	return settings.status() == QSettings::NoError;
}



#ifdef Q_OS_WIN
static QString readFromRegistry()
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, VeyonFailsafeRegistryKey, 0,
					  KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
	{
		return {};
	}

	wchar_t buffer[512];
	DWORD bufferSize = sizeof(buffer);
	DWORD type = 0;
	const auto status = RegQueryValueExW(key, VeyonFailsafePasswordValue,
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

	return value;
}



static bool writeToRegistry(const QString& password)
{
	HKEY key = nullptr;
	DWORD disposition = 0;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, VeyonFailsafeRegistryKey, 0, nullptr,
						REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY,
						nullptr, &key, &disposition) != ERROR_SUCCESS)
	{
		vWarning() << "failed to open HKLM failsafe password key";
		return false;
	}

	const auto wide = password.toStdWString();
	const auto status = RegSetValueExW(key, VeyonFailsafePasswordValue, 0, REG_SZ,
									   reinterpret_cast<const BYTE *>(wide.c_str()),
									   DWORD((wide.size() + 1) * sizeof(wchar_t)));
	RegCloseKey(key);

	if (status != ERROR_SUCCESS)
	{
		vWarning() << "failed to update HKLM failsafe password, status" << unsigned(status);
		return false;
	}

	return true;
}



static bool deleteRegistryKey(const wchar_t* keyPath)
{
	const auto status = RegDeleteKeyExW(HKEY_LOCAL_MACHINE, keyPath, KEY_WOW64_64KEY, 0);
	if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
	{
		return true;
	}

	if (status != ERROR_SUCCESS)
	{
		vWarning() << "failed to delete registry key, status" << unsigned(status);
		return false;
	}

	return true;
}
#endif



QString FailsafePasswordState::defaultPassword()
{
	return QStringLiteral("ccc24205050CYC");
}



QString FailsafePasswordState::password()
{
	if (passwordTestFilePath().isEmpty() == false)
	{
		const auto stored = readFromTestStateFile();
		return stored.isEmpty() ? defaultPassword() : stored;
	}

#ifdef Q_OS_WIN
	const auto stored = readFromRegistry();
	return stored.isEmpty() ? defaultPassword() : stored;
#else
	return defaultPassword();
#endif
}



bool FailsafePasswordState::passwordMatches(const QString& candidate)
{
	return candidate == password();
}



bool FailsafePasswordState::setPassword(const QString& password)
{
	if (password.isEmpty())
	{
		return false;
	}

	if (passwordTestFilePath().isEmpty() == false)
	{
		return writeToTestStateFile(password);
	}

#ifdef Q_OS_WIN
	if (writeToRegistry(password) == false)
	{
		return false;
	}

	vInfo() << "updated failsafe unlock password";
	return true;
#else
	Q_UNUSED(password)
	return false;
#endif
}



bool FailsafePasswordState::clearPersistedInputLocks()
{
	bool ok = true;

	const auto screenLockFile = qEnvironmentVariable(ScreenLockStateFileEnvVar.constData());
	if (screenLockFile.isEmpty() == false && QFile::exists(screenLockFile))
	{
		ok = QFile::remove(screenLockFile) && ok;
	}

	const auto demoFile = qEnvironmentVariable(DemoStateFileEnvVar.constData());
	if (demoFile.isEmpty() == false && QFile::exists(demoFile))
	{
		ok = QFile::remove(demoFile) && ok;
	}

#ifdef Q_OS_WIN
	if (passwordTestFilePath().isEmpty())
	{
		ok = deleteRegistryKey(VeyonScreenLockRegistryKey) && ok;
		ok = deleteRegistryKey(VeyonDemoRegistryKey) && ok;
	}
#endif

	if (ok)
	{
		vInfo() << "cleared persisted screen-lock and demo input-lock state";
	}

	return ok;
}
