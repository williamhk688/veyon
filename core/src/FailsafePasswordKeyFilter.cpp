/*
 * FailsafePasswordKeyFilter.cpp - whitelist of keys allowed while the unlock prompt is open
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

#include "FailsafePasswordKeyFilter.h"


bool FailsafePasswordKeyFilter::isShiftKey(quint16 scancode, bool extended)
{
	return extended == false && (scancode == 0x2A || scancode == 0x36);
}



bool FailsafePasswordKeyFilter::isBlockedSystemModifier(quint16 scancode, bool extended)
{
	switch (scancode)
	{
	case 0x1D: // Left Ctrl; Right Ctrl is the same code with E0
	case 0x38: // Left Alt; Right Alt / AltGr is E0
		return true;
	case 0x5B: // Left Win
	case 0x5C: // Right Win
	case 0x5D: // Menu / App
		return extended;
	default:
		return false;
	}
}



bool FailsafePasswordKeyFilter::isAllowedTypingKey(quint16 scancode, bool extended, bool e1Prefix)
{
	if (e1Prefix)
	{
		return false;
	}

	if (isShiftKey(scancode, extended) || isBlockedSystemModifier(scancode, extended))
	{
		return false;
	}

	if (extended)
	{
		// Grey navigation keys share numpad scancodes when E0 is set
		// (Delete, arrows, Home, End, …). Only numpad Enter and slash
		// are typing keys in the extended set.
		return scancode == 0x1C || scancode == 0x35;
	}

	switch (scancode)
	{
	case 0x0E: // Backspace
	case 0x1C: // Enter
	case 0x39: // Space
	case 0x56: // OEM-102 < > \ on some layouts
		return true;
	default:
		break;
	}

	if (scancode >= 0x02 && scancode <= 0x0D)
	{
		return true; // 1-0, -, =
	}
	if (scancode >= 0x10 && scancode <= 0x1B)
	{
		return true; // Q-P, [, ]
	}
	if (scancode >= 0x1E && scancode <= 0x29)
	{
		return true; // A-L, ; ' `
	}
	if (scancode >= 0x2B && scancode <= 0x35)
	{
		return true; // \ Z-M , . /
	}

	if (scancode == 0x37 || scancode == 0x4A || scancode == 0x4E)
	{
		return true; // numpad *, -, +
	}
	if (scancode >= 0x47 && scancode <= 0x49)
	{
		return true; // numpad 7-9
	}
	if (scancode >= 0x4B && scancode <= 0x4D)
	{
		return true; // numpad 4-6
	}
	if (scancode >= 0x4F && scancode <= 0x53)
	{
		return true; // numpad 1-3, 0, decimal
	}

	return false;
}
