/*
 * FailsafePasswordKeyFilter.h - whitelist of keys allowed while the unlock prompt is open
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

#include "VeyonCore.h"

/*!
 * Keyboard whitelist used while the failsafe password dialog is visible.
 *
 * Interception keeps FILTER_KEY_ALL. Only typing keys are re-injected:
 * letters, digits, OEM symbols, Space, Enter, and Backspace.
 *
 * Shift is allowed as a character modifier (uppercase / !@#…) but is
 * never delivered as an isolated key. That still blocks:
 *   - Ctrl+Alt+Del (Ctrl/Alt/Del are not on the whitelist)
 *   - Win / Win+X / Win+L (Win is not on the whitelist)
 *   - Ctrl+Shift+Esc Task Manager (Ctrl and Esc are not on the whitelist)
 *   - Sticky Keys (five isolated Shift taps never reach Windows)
 *   - Filter Keys (eight-second Right Shift hold never reaches Windows)
 *
 * Ctrl, Alt, Win, Esc, Tab, Delete, function keys and arrows stay swallowed.
 */
class VEYON_CORE_EXPORT FailsafePasswordKeyFilter
{
public:
	static bool isAllowedTypingKey(quint16 scancode, bool extended, bool e1Prefix = false);
	static bool isShiftKey(quint16 scancode, bool extended);
	static bool isBlockedSystemModifier(quint16 scancode, bool extended);
};
