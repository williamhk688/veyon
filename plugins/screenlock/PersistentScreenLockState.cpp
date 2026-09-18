/*
 * PersistentScreenLockState.cpp - persist teacher screen-lock across reboot
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

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopedPointer>
#include <QSettings>
#include <QTextStream>

#include "PersistentScreenLockState.h"
#include "PlatformFilesystemFunctions.h"
#include "VeyonCore.h"


static const auto SettingsApplicationName = QStringLiteral("VeyonScreenLock");
static const auto LockedFeatureUidKey = QStringLiteral("LockedFeatureUid");
static const auto StateFileName = QStringLiteral("screenlock.state");
static const auto StateFileEnvVar = QByteArrayLiteral("VEYON_SCREENLOCK_STATE_FILE");


static QSettings* createSystemSettings()
{
	return new QSettings(
#ifdef Q_OS_WIN
				QSettings::Registry64Format,
#else
				QSettings::NativeFormat,
#endif
				QSettings::SystemScope,
				QCoreApplication::organizationName(),
				SettingsApplicationName);
}



QString PersistentScreenLockState::stateFilePath()
{
	const auto overridePath = qEnvironmentVariable(StateFileEnvVar.constData());
	if (overridePath.isEmpty() == false)
	{
		return overridePath;
	}

	return VeyonCore::platform().filesystemFunctions().globalAppDataPath() +
			QDir::separator() + StateFileName;
}



bool PersistentScreenLockState::useFileOnly()
{
	return qEnvironmentVariableIsSet(StateFileEnvVar.constData());
}



Feature::Uid PersistentScreenLockState::readFromSettings()
{
	QScopedPointer<QSettings> settings(createSystemSettings());
	settings->setFallbacksEnabled(false);
	const auto value = settings->value(LockedFeatureUidKey).toString();
	const Feature::Uid uid{value};
	return uid.isNull() ? Feature::Uid{} : uid;
}



Feature::Uid PersistentScreenLockState::readFromStateFile()
{
	const auto path = stateFilePath();
	if (QFile::exists(path) == false)
	{
		return {};
	}

	QSettings settings(path, QSettings::IniFormat);
	settings.setFallbacksEnabled(false);
	const Feature::Uid uid{settings.value(LockedFeatureUidKey).toString()};
	return uid.isNull() ? Feature::Uid{} : uid;
}



bool PersistentScreenLockState::writeToSettings(const Feature::Uid& featureUid)
{
	QScopedPointer<QSettings> settings(createSystemSettings());
	settings->setFallbacksEnabled(false);
	if (featureUid.isNull())
	{
		settings->remove(LockedFeatureUidKey);
	}
	else
	{
		settings->setValue(LockedFeatureUidKey, featureUid.toString(QUuid::WithoutBraces));
	}
	settings->sync();

	if (settings->status() != QSettings::NoError)
	{
		vWarning() << "failed to persist screen lock in system settings, status" << settings->status();
		return false;
	}

	return true;
}



bool PersistentScreenLockState::writeToStateFile(const Feature::Uid& featureUid)
{
	const auto path = stateFilePath();
	const auto directory = QFileInfo(path).absolutePath();
	if (QDir().mkpath(directory) == false)
	{
		vWarning() << "failed to create screen lock state directory" << directory;
		return false;
	}

	if (featureUid.isNull())
	{
		if (QFile::exists(path) && QFile::remove(path) == false)
		{
			vWarning() << "failed to remove screen lock state file" << path;
			return false;
		}
		return true;
	}

	QFile file(path);
	const auto permissions = QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther;
	auto opened = false;

	if (VeyonCore::instance())
	{
		opened = VeyonCore::platform().filesystemFunctions().openFileSafely(
					&file, QFile::WriteOnly | QFile::Truncate | QFile::Text, permissions);
	}

	if (opened == false)
	{
		opened = file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text);
		if (opened)
		{
			file.setPermissions(permissions);
		}
	}

	if (opened == false)
	{
		vWarning() << "failed to write screen lock state file" << path;
		return false;
	}

	QTextStream stream(&file);
	stream << QStringLiteral("[%1]\n%2=%3\n")
			  .arg(QStringLiteral("General"), LockedFeatureUidKey, featureUid.toString(QUuid::WithoutBraces));
	return stream.status() == QTextStream::Ok;
}



Feature::Uid PersistentScreenLockState::lockedFeatureUid()
{
	const auto fromFile = readFromStateFile();
	if (fromFile.isNull() == false)
	{
		return fromFile;
	}

	if (useFileOnly())
	{
		return {};
	}

	return readFromSettings();
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

	auto ok = writeToStateFile(featureUid);
	if (useFileOnly() == false)
	{
		ok = writeToSettings(featureUid) || ok;
	}

	if (ok)
	{
		vInfo() << "persisted screen lock" << featureUid;
	}

	return ok;
}



bool PersistentScreenLockState::clear()
{
	auto ok = writeToStateFile({});
	if (useFileOnly() == false)
	{
		ok = writeToSettings({}) && ok;
	}

	if (ok)
	{
		vInfo() << "cleared persisted screen lock";
	}

	return ok;
}
