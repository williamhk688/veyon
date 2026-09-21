/*
 * VeyonFailsafeRegistry.h - HKLM location for the failsafe unlock password
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
 * Windows 10/11 registry location for the teacher failsafe unlock password.
 *
 * Sibling of Configuration::LocalStore
 *   HKLM\SOFTWARE\Veyon Solutions\Veyon
 * and of VeyonScreenLock / VeyonDemo so Configurator flush/clear cannot
 * wipe the password.
 *
 * Open with KEY_WOW64_64KEY to match QSettings::Registry64Format.
 *
 * Value name FailsafePassword is a REG_SZ. When the value is missing the
 * built-in default from FailsafePasswordState is used.
 */
inline constexpr wchar_t VeyonFailsafeRegistryKey[] =
	L"SOFTWARE\\Veyon Solutions\\VeyonFailsafe";
inline constexpr wchar_t VeyonFailsafePasswordValue[] = L"FailsafePassword";
