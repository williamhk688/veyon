/*
 * VeyonScreenLockRegistry.h - HKLM location for Windows screen-lock persistence
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

#pragma once

/*!
 * Windows 10/11 registry location for a teacher screen-lock that must
 * survive reboot.
 *
 * Sibling of Configuration::LocalStore
 *   HKLM\SOFTWARE\Veyon Solutions\Veyon
 * so Configurator flush/clear cannot wipe the lock flag.
 *
 * Open with KEY_WOW64_64KEY to match QSettings::Registry64Format.
 */
inline constexpr wchar_t VeyonScreenLockRegistryKey[] =
	L"SOFTWARE\\Veyon Solutions\\VeyonScreenLock";
inline constexpr wchar_t VeyonScreenLockRegistryValue[] = L"LockedFeatureUid";
