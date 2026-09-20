/*
 * FailsafeUnlock.h - hidden hotkey prompt used by LockWidget / DemoClient
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

#include <QObject>

#include "VeyonCore.h"

class QKeyEvent;
class QWidget;

/*!
 * Cross-process hotkey bridge. On Windows the Interception receive thread
 * (and a Global named event) notify this object so LockWidget can prompt
 * even while FILTER_KEY_ALL swallows ordinary Qt key events.
 */
class VEYON_CORE_EXPORT FailsafeHotkeyMonitor : public QObject
{
	Q_OBJECT
public:
	static FailsafeHotkeyMonitor& instance();

	void notifyHotkeyPressed();
	void setPasswordPromptPassthrough(bool enabled);
	bool passwordPromptPassthrough() const;

	static bool isUnlockHotkey(const QKeyEvent* event);

Q_SIGNALS:
	void hotkeyPressed();

private:
	explicit FailsafeHotkeyMonitor();

#ifdef Q_OS_WIN
	void* m_hotkeyEvent{nullptr};
	void* m_passthroughEvent{nullptr};
	class QWinEventNotifier* m_hotkeyNotifier{nullptr};
#endif
};

class VEYON_CORE_EXPORT FailsafeUnlock
{
public:
	static constexpr auto HotkeySequence = "Ctrl+Alt+Shift+U";

	/*!
	 * Shows a password-masked QInputDialog, compares against the registry
	 * (or the built-in default), and on success clears persisted lock/demo
	 * keys. Returns true only when the password matched.
	 */
	static bool prompt(QWidget* parent);
};
