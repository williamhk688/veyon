/*
 * ClassroomPersistStore.cpp - ProgramData mirror of lock/demo/web-filter flags
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

#include "ClassroomPersistStore.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif


static const auto PersistDirEnvVar = QByteArrayLiteral("VEYON_CLASSROOM_PERSIST_DIR");


QString ClassroomPersistStore::directory()
{
	const auto overrideDir = qEnvironmentVariable(PersistDirEnvVar.constData());
	if (overrideDir.isEmpty() == false)
	{
		return overrideDir;
	}

	const auto root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	if (root.isEmpty())
	{
		return {};
	}
	return QDir(root).filePath(QStringLiteral("Veyon/classroom-persist"));
}


QString ClassroomPersistStore::screenLockName()
{
	return QStringLiteral("screenlock-uid");
}


QString ClassroomPersistStore::demoName()
{
	return QStringLiteral("demo.json");
}


QString ClassroomPersistStore::webFilterSessionName()
{
	return QStringLiteral("webfilter-session.json");
}


QString ClassroomPersistStore::webFilterModeName()
{
	return QStringLiteral("webfilter-mode");
}


QString ClassroomPersistStore::webFilterDomainsName()
{
	return QStringLiteral("webfilter-domains");
}


bool ClassroomPersistStore::writeValue(const QString& name, const QString& text)
{
	const auto dir = directory();
	if (dir.isEmpty() || name.isEmpty())
	{
		return false;
	}

	if (QDir().mkpath(dir) == false)
	{
		return false;
	}

	if (text.isEmpty())
	{
		return removeValue(name);
	}

	const auto path = QDir(dir).filePath(name);
	QSaveFile file(path);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate) == false)
	{
		return false;
	}
	file.write(text.toUtf8());
	if (file.commit() == false)
	{
		return false;
	}

#ifdef Q_OS_WIN
	const auto handle = CreateFileW(QDir::toNativeSeparators(path).toStdWString().c_str(),
									GENERIC_READ, FILE_SHARE_READ, nullptr,
									OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle != INVALID_HANDLE_VALUE)
	{
		FlushFileBuffers(handle);
		CloseHandle(handle);
	}
#endif
	return true;
}


QString ClassroomPersistStore::readValue(const QString& name)
{
	const auto dir = directory();
	if (dir.isEmpty() || name.isEmpty())
	{
		return {};
	}

	QFile file(QDir(dir).filePath(name));
	if (file.open(QIODevice::ReadOnly) == false)
	{
		return {};
	}
	return QString::fromUtf8(file.readAll()).trimmed();
}


bool ClassroomPersistStore::removeValue(const QString& name)
{
	const auto dir = directory();
	if (dir.isEmpty() || name.isEmpty())
	{
		return true;
	}

	const auto path = QDir(dir).filePath(name);
	return QFile::exists(path) == false || QFile::remove(path);
}


bool ClassroomPersistStore::removeClassroomLocks()
{
	return removeValue(screenLockName()) && removeValue(demoName());
}


bool ClassroomPersistStore::removeWebFilter()
{
	return removeValue(webFilterSessionName()) &&
			removeValue(webFilterModeName()) &&
			removeValue(webFilterDomainsName());
}
