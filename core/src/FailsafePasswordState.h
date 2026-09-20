/*
 * FailsafePasswordState.h - failsafe unlock password and lock-clear helpers
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
 * Teacher failsafe password used to unlock a persisted screen-lock or demo.
 *
 * Production storage is HKLM (64-bit view):
 *   HKLM\SOFTWARE\Veyon Solutions\VeyonFailsafe
 *   REG_SZ FailsafePassword = "<password>"
 *
 * When the value is missing, DefaultPassword() is used.
 *
 * On non-Windows builds every method is a no-op unless
 * VEYON_FAILSAFE_PASSWORD_FILE is set (unit tests only).
 */
class VEYON_CORE_EXPORT FailsafePasswordState
{
public:
	static QString defaultPassword();
	static QString password();
	static bool passwordMatches(const QString& candidate);
	static bool setPassword(const QString& password);

	/*!
	 * Master-side form checks: current and new must be non-empty, the two
	 * new-password fields must match, and the new value must differ from
	 * the current one. Does not consult the stored password.
	 */
	static bool validatePasswordChangeInput(const QString& currentPassword,
											const QString& newPassword,
											const QString& confirmation);

	/*!
	 * Student-side change: succeeds only when currentPassword matches the
	 * stored (or default) value, then writes newPassword.
	 */
	static bool changePassword(const QString& currentPassword, const QString& newPassword);

	/*!
	 * Master-only cache of the last password that at least one student
	 * stored successfully. Used by the gated self-rescue handbook. Does not
	 * replace HKLM on student PCs.
	 */
	static bool rememberPassword(const QString& password);
	static QString handbookPassword();

	/*!
	 * Deletes the persisted screen-lock and demo input-lock flags so the
	 * Windows service registry watch re-enables input after a local unlock.
	 */
	static bool clearPersistedInputLocks();
};
