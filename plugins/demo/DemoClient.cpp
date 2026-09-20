/*
 * DemoClient.cpp - client widget for demo mode
 *
 * Copyright (c) 2006-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QApplication>
#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QShortcut>

#include "DemoClient.h"
#include "FailsafeUnlock.h"
#include "LockWidget.h"
#include "PlatformCoreFunctions.h"
#include "VncViewWidget.h"


DemoClient::DemoClient( const QString& host, int port, bool fullscreen, QRect viewport, QObject* parent ) :
	QObject( parent ),
	m_computerControlInterface( ComputerControlInterface::Pointer::create( Computer( {}, host, host ), port, this ) )
{
	if( fullscreen )
	{
		m_toplevel = new LockWidget( LockWidget::NoBackground );
	}
	else
	{
		m_toplevel = new QWidget();
		m_toplevel->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);
		m_toplevel->move(0, 0);
	}

	m_toplevel->setWindowTitle(tr("Veyon Demo"));
	m_toplevel->setWindowIcon( QPixmap( QStringLiteral(":/core/icon64.png") ) );
	m_toplevel->setAttribute( Qt::WA_DeleteOnClose, false );
	m_toplevel->installEventFilter(this);

	if (auto* lockWidget = qobject_cast<LockWidget *>(m_toplevel))
	{
		connect(lockWidget, &LockWidget::failsafeUnlocked, this, &DemoClient::failsafeUnlocked);
	}
	else
	{
		auto* failsafeShortcut = new QShortcut(QKeySequence(QLatin1String(FailsafeUnlock::HotkeySequence)), m_toplevel);
		failsafeShortcut->setContext(Qt::ApplicationShortcut);
		connect(failsafeShortcut, &QShortcut::activated, this, &DemoClient::promptFailsafeUnlock);
		connect(&FailsafeHotkeyMonitor::instance(), &FailsafeHotkeyMonitor::hotkeyPressed,
				this, &DemoClient::promptFailsafeUnlock);
	}

	m_vncView = new VncViewWidget( m_computerControlInterface, viewport, m_toplevel );

	connect( m_toplevel, &QObject::destroyed, this, &DemoClient::viewDestroyed );
	connect( m_vncView, &VncViewWidget::sizeHintChanged, this, &DemoClient::resizeToplevelWidget );

	if (fullscreen == false)
	{
		m_toplevel->show();
	}

	resizeToplevelWidget();

	VeyonCore::platform().coreFunctions().raiseWindow( m_toplevel, fullscreen );

	VeyonCore::platform().coreFunctions().disableScreenSaver();
}



DemoClient::~DemoClient()
{
	VeyonCore::platform().coreFunctions().restoreScreenSaverSettings();

	delete m_toplevel;
}



bool DemoClient::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == m_toplevel && event->type() == QEvent::Resize)
	{
		m_vncView->setFixedSize(m_toplevel->size());
		return true;
	}

	if (watched == m_toplevel && event->type() == QEvent::KeyPress &&
		qobject_cast<LockWidget *>(m_toplevel) == nullptr)
	{
		if (FailsafeHotkeyMonitor::isUnlockHotkey(static_cast<QKeyEvent *>(event)))
		{
			promptFailsafeUnlock();
			return true;
		}
	}

	return QObject::eventFilter(watched, event);
}



void DemoClient::viewDestroyed( QObject* obj )
{
	// prevent double deletion of toplevel widget
	if( m_toplevel == obj )
	{
		m_toplevel = nullptr;
	}

	deleteLater();
}



void DemoClient::resizeToplevelWidget()
{
	if( m_toplevel->windowState() & Qt::WindowFullScreen )
	{
		m_vncView->setFixedSize(m_toplevel->size());
	}
	else
	{
		m_toplevel->resize(m_vncView->sizeHint());
	}
}



void DemoClient::promptFailsafeUnlock()
{
	if (m_failsafePromptOpen || m_toplevel == nullptr)
	{
		return;
	}

	m_failsafePromptOpen = true;
	if (FailsafeUnlock::prompt(m_toplevel))
	{
		Q_EMIT failsafeUnlocked();
		return;
	}

	m_failsafePromptOpen = false;
}
