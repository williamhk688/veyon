/*
 * FailsafeUnlock.cpp - hidden hotkey prompt used by LockWidget / DemoClient
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
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMutex>
#include <QThread>
#include <QWidget>

#include "FailsafePasswordState.h"
#include "FailsafeUnlock.h"
#include "Logger.h"
#include "PlatformCoreFunctions.h"
#include "PlatformPluginInterface.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <QWinEventNotifier>
#endif


static QMutex& monitorMutex()
{
	static QMutex mutex;
	return mutex;
}


#ifdef Q_OS_WIN
static constexpr auto HotkeyEventName = L"Global\\VeyonFailsafeHotkey";
static constexpr auto PassthroughEventName = L"Global\\VeyonFailsafePasswordDialogActive";


static HANDLE createGlobalEvent(const wchar_t* name, bool manualReset)
{
	SECURITY_DESCRIPTOR securityDescriptor;
	InitializeSecurityDescriptor(&securityDescriptor, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&securityDescriptor, TRUE, nullptr, FALSE);

	SECURITY_ATTRIBUTES securityAttributes;
	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.lpSecurityDescriptor = &securityDescriptor;
	securityAttributes.bInheritHandle = FALSE;

	auto handle = CreateEventW(&securityAttributes, manualReset ? TRUE : FALSE, FALSE, name);
	if (handle == nullptr)
	{
		handle = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, name);
	}

	return handle;
}
#endif



FailsafeHotkeyMonitor::FailsafeHotkeyMonitor() :
	QObject(nullptr)
{
#ifdef Q_OS_WIN
	m_hotkeyEvent = createGlobalEvent(HotkeyEventName, false);
	m_passthroughEvent = createGlobalEvent(PassthroughEventName, true);

	if (m_hotkeyEvent)
	{
		m_hotkeyNotifier = new QWinEventNotifier(static_cast<HANDLE>(m_hotkeyEvent), this);
		connect(m_hotkeyNotifier, &QWinEventNotifier::activated, this, [this]() {
			Q_EMIT hotkeyPressed();
		});
	}
#endif
}



FailsafeHotkeyMonitor& FailsafeHotkeyMonitor::instance()
{
	static FailsafeHotkeyMonitor* monitor = nullptr;
	QMutexLocker locker(&monitorMutex());
	if (monitor)
	{
		return *monitor;
	}

	auto* app = QCoreApplication::instance();
	if (app && QThread::currentThread() != app->thread())
	{
		locker.unlock();
		QMetaObject::invokeMethod(app, []() {
			QMutexLocker inner(&monitorMutex());
			if (monitor == nullptr)
			{
				monitor = new FailsafeHotkeyMonitor;
			}
		}, Qt::BlockingQueuedConnection);
		return *monitor;
	}

	monitor = new FailsafeHotkeyMonitor;
	return *monitor;
}



void FailsafeHotkeyMonitor::notifyHotkeyPressed()
{
	QMetaObject::invokeMethod(this, [this]() {
		Q_EMIT hotkeyPressed();
#ifdef Q_OS_WIN
		if (m_hotkeyEvent)
		{
			SetEvent(static_cast<HANDLE>(m_hotkeyEvent));
		}
#endif
	}, Qt::QueuedConnection);
}



void FailsafeHotkeyMonitor::setPasswordPromptPassthrough(bool enabled)
{
#ifdef Q_OS_WIN
	if (m_passthroughEvent == nullptr)
	{
		return;
	}

	if (enabled)
	{
		SetEvent(static_cast<HANDLE>(m_passthroughEvent));
	}
	else
	{
		ResetEvent(static_cast<HANDLE>(m_passthroughEvent));
	}
#else
	Q_UNUSED(enabled)
#endif
}



bool FailsafeHotkeyMonitor::passwordPromptPassthrough() const
{
#ifdef Q_OS_WIN
	if (m_passthroughEvent == nullptr)
	{
		return false;
	}

	return WaitForSingleObject(static_cast<HANDLE>(m_passthroughEvent), 0) == WAIT_OBJECT_0;
#else
	return false;
#endif
}



bool FailsafeHotkeyMonitor::isUnlockHotkey(const QKeyEvent* event)
{
	if (event == nullptr)
	{
		return false;
	}

	const auto mods = event->modifiers() & (Qt::ControlModifier | Qt::AltModifier |
											Qt::ShiftModifier | Qt::MetaModifier);
	return mods == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier) &&
			event->key() == Qt::Key_U;
}



bool FailsafeUnlock::prompt(QWidget* parent)
{
	static bool dialogOpen = false;
	if (dialogOpen)
	{
		return false;
	}

	dialogOpen = true;
	FailsafeHotkeyMonitor::instance().setPasswordPromptPassthrough(true);

	QInputDialog dialog(parent);
	dialog.setWindowTitle(QCoreApplication::translate("FailsafeUnlock", "Unlock"));
	dialog.setLabelText(QCoreApplication::translate("FailsafeUnlock",
													"Enter the failsafe unlock password:"));
	dialog.setTextEchoMode(QLineEdit::Password);
	dialog.setWindowModality(Qt::ApplicationModal);
	VeyonCore::platform().coreFunctions().raiseWindow(&dialog, true);

	if (dialog.exec() != QDialog::Accepted)
	{
		FailsafeHotkeyMonitor::instance().setPasswordPromptPassthrough(false);
		dialogOpen = false;
		return false;
	}

	const auto entered = dialog.textValue();

	if (FailsafePasswordState::passwordMatches(entered) == false)
	{
		QMessageBox::warning(parent,
							 QCoreApplication::translate("FailsafeUnlock", "Unlock"),
							 QCoreApplication::translate("FailsafeUnlock", "The password is incorrect."));
		FailsafeHotkeyMonitor::instance().setPasswordPromptPassthrough(false);
		dialogOpen = false;
		return false;
	}

	FailsafePasswordState::clearPersistedInputLocks();
	FailsafeHotkeyMonitor::instance().setPasswordPromptPassthrough(false);
	dialogOpen = false;
	vInfo() << "failsafe unlock succeeded";
	return true;
}
