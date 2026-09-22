/*
 *  LockWidget.cpp - widget for locking a client
 *
 *  Copyright (c) 2006-2026 Tobias Junghans <tobydox@veyon.io>
 *
 *  This file is part of Veyon - https://veyon.io
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include "LockWidget.h"
#include "FailsafeUnlock.h"
#include "PlatformCoreFunctions.h"
#include "PlatformInputDeviceFunctions.h"

#include <QApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QShortcut>
#include <QWindow>


LockWidget::LockWidget( Mode mode, const QPixmap& background, QWidget* parent ) :
	QWidget( parent, Qt::X11BypassWindowManagerHint ),
	m_background( background ),
	m_mode( mode )
{
	auto leftMostScreen = QGuiApplication::primaryScreen();
	int minimumX = 0;
	const auto screens = QGuiApplication::screens();
	for (auto* screen : screens)
	{
		if (screen->geometry().x() < minimumX)
		{
			minimumX = screen->geometry().x();
			leftMostScreen = screen;
		}
	}

	if (mode == DesktopVisible)
	{
		m_background = leftMostScreen->grabWindow(0);
	}

	VeyonCore::platform().coreFunctions().setSystemUiState( false );
	VeyonCore::platform().inputDeviceFunctions().disableInputDevices();

	setWindowTitle( {} );

#ifdef Q_OS_LINUX
	show();
#endif
	move(leftMostScreen->geometry().topLeft());
#ifndef Q_OS_LINUX
	showFullScreen();
#endif
	windowHandle()->setScreen(leftMostScreen);
	setFixedSize(leftMostScreen->virtualSize());

	VeyonCore::platform().coreFunctions().raiseWindow(this, true);
#ifdef Q_OS_LINUX
	showFullScreen();
#endif

	setFocusPolicy( Qt::StrongFocus );
	setFocus();
	grabMouse();
	grabKeyboard();
	setCursor( Qt::BlankCursor );
	QGuiApplication::setOverrideCursor( Qt::BlankCursor );

	QCursor::setPos( mapToGlobal( QPoint( 0, 0 ) ) );

	auto* failsafeShortcut = new QShortcut(QKeySequence(QLatin1String(FailsafeUnlock::HotkeySequence)), this);
	failsafeShortcut->setContext(Qt::ApplicationShortcut);
	connect(failsafeShortcut, &QShortcut::activated, this, &LockWidget::promptFailsafeUnlock);
	connect(&FailsafeHotkeyMonitor::instance(), &FailsafeHotkeyMonitor::hotkeyPressed,
			this, &LockWidget::promptFailsafeUnlock);
}



LockWidget::~LockWidget()
{
	VeyonCore::platform().inputDeviceFunctions().enableInputDevices();
	VeyonCore::platform().coreFunctions().setSystemUiState( true );

	QGuiApplication::restoreOverrideCursor();
}



void LockWidget::paintEvent( QPaintEvent* event )
{
	Q_UNUSED(event);

	QPainter p( this );
	switch( m_mode )
	{
	case DesktopVisible:
		p.drawPixmap( 0, 0, m_background );
		break;

	case BackgroundPixmap:
		p.fillRect( rect(), Qt::white );
		p.drawPixmap( ( width() - m_background.width() ) / 2,
					  ( height() - m_background.height() ) / 2,
					  m_background );
		break;

	default:
		break;
	}
}



void LockWidget::keyPressEvent( QKeyEvent* event )
{
	if (FailsafeHotkeyMonitor::isUnlockHotkey(event))
	{
		promptFailsafeUnlock();
		return;
	}

	QWidget::keyPressEvent(event);
}



void LockWidget::promptFailsafeUnlock()
{
	if (m_failsafePromptOpen)
	{
		return;
	}

	m_failsafePromptOpen = true;
	releaseKeyboard();
	releaseMouse();
	QGuiApplication::restoreOverrideCursor();
	unsetCursor();

	if (FailsafeUnlock::prompt(this))
	{
		Q_EMIT failsafeUnlocked();
		return;
	}

	restoreInputGrab();
	m_failsafePromptOpen = false;
}



void LockWidget::restoreInputGrab()
{
	setCursor( Qt::BlankCursor );
	QGuiApplication::setOverrideCursor( Qt::BlankCursor );
	grabMouse();
	grabKeyboard();
	setFocus();
	VeyonCore::platform().coreFunctions().raiseWindow(this, true);
}
