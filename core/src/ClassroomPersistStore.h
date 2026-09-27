/*
 * ClassroomPersistStore.h - ProgramData mirror of lock/demo/web-filter flags
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QString>
#include <QtGlobal>

#if !defined(VEYON_CORE_EXPORT)
#  if defined(veyon_core_EXPORTS)
#    define VEYON_CORE_EXPORT Q_DECL_EXPORT
#  else
#    define VEYON_CORE_EXPORT Q_DECL_IMPORT
#  endif
#endif

/*!
 * Disk backup next to HKLM so a reboot still restores classroom lock,
 * demo, and web filter if a registry value is missing or flushed late.
 *
 * Default directory: %ProgramData%/Veyon/classroom-persist
 */
class VEYON_CORE_EXPORT ClassroomPersistStore
{
public:
	static QString directory();
	static QString screenLockName();
	static QString demoName();
	static QString webFilterSessionName();
	static QString webFilterModeName();
	static QString webFilterDomainsName();

	static bool writeValue(const QString& name, const QString& text);
	static QString readValue(const QString& name);
	static bool removeValue(const QString& name);
	static bool removeClassroomLocks();
	static bool removeWebFilter();
};
