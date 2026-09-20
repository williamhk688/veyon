/*
 * ScreenLockFeaturePlugin.cpp - implementation of ScreenLockFeaturePlugin class
 *
 * Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
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
#include <QTimer>

#include "ScreenLockFeaturePlugin.h"
#include "ComputerControlInterface.h"
#include "FailsafePasswordState.h"
#include "FeatureWorkerManager.h"
#include "LockWidget.h"
#include "PersistentScreenLockState.h"
#include "PlatformCoreFunctions.h"
#include "PlatformInputDeviceFunctions.h"
#include "PlatformSessionFunctions.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"


ScreenLockFeaturePlugin::ScreenLockFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_screenLockFeature( QStringLiteral( "ScreenLock" ),
						 Feature::Flag::Mode | Feature::Flag::AllComponents,
						 Feature::Uid( "ccb535a2-1d24-4cc1-a709-8b47d2b2ac79" ),
						 Feature::Uid(),
						 tr( "Lock" ), tr( "Unlock" ),
						 tr( "To reclaim all user's full attention you can lock "
							 "their computers using this button. "
							 "In this mode all input devices are locked and "
							 "the screens are blacked." ),
						 QStringLiteral(":/screenlock/system-lock-screen.png") ),
	m_lockInputDevicesFeature( QStringLiteral( "InputDevicesLock" ),
							   Feature::Flag::Mode | Feature::Flag::AllComponents | Feature::Flag::Meta,
							   Feature::Uid( "e4a77879-e544-4fec-bc18-e534f33b934c" ),
							   {},
							   tr( "Lock input devices" ), tr( "Unlock input devices" ),
							   tr( "To reclaim all user's full attention you can lock "
								   "their computers using this button. "
								   "In this mode all input devices are locked while the desktop is still visible." ),
							   QStringLiteral(":/screenlock/system-lock-screen.png") ),
	m_features( { m_screenLockFeature, m_lockInputDevicesFeature } ),
	m_lockWidget( nullptr )
{
	if (VeyonCore::component() == VeyonCore::Component::Service)
	{
		connect (VeyonCore::instance(), &VeyonCore::initialized,
				 this, []() {
#ifdef Q_OS_WIN
			// Windows service (LocalSystem) starts before the user desktop.
			// If HKLM still says locked, disable input at driver/HID level so
			// the machine cannot be used until Master unlocks.
			if (PersistentScreenLockState::isLocked())
			{
				VeyonCore::platform().inputDeviceFunctions().disableInputDevices();
				return;
			}
#endif
			VeyonCore::platform().inputDeviceFunctions().enableInputDevices();
		});
	}
}



ScreenLockFeaturePlugin::~ScreenLockFeaturePlugin()
{
	delete m_lockWidget;
}



bool ScreenLockFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
											 const QVariantMap& arguments,
											 const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(arguments)

	if( hasFeature( featureUid ) == false )
	{
		return false;
	}

	if( operation == Operation::Start )
	{
		// never lock the master computer itself: in multi-teacher lab layouts it
		// is listed as a regular room computer, but locking its screen would grab
		// the teacher's keyboard and mouse and cover Veyon Master, leaving no way
		// to unlock the running session
		auto lockControlInterfaces = computerControlInterfaces;
		lockControlInterfaces.removeLocalHostInterfaces();

		sendFeatureMessage(FeatureMessage{featureUid, FeatureCommand::StartLock}, lockControlInterfaces);

		return true;
	}

	if( operation == Operation::Stop )
	{
		sendFeatureMessage(FeatureMessage{featureUid, FeatureCommand::StopLock}, computerControlInterfaces);

		return true;
	}

	return false;
}



bool ScreenLockFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
													const MessageContext& messageContext,
													const FeatureMessage& message )
{
	Q_UNUSED(messageContext)

	if( message.featureUid() == m_screenLockFeature.uid() ||
		message.featureUid() == m_lockInputDevicesFeature.uid() )
	{
		if (message.command<FeatureCommand>() == FeatureCommand::StopLock)
		{
#ifdef Q_OS_WIN
			if (PersistentScreenLockState::lockedFeatureUid() == message.featureUid())
			{
				PersistentScreenLockState::clear();
			}
			VeyonCore::platform().inputDeviceFunctions().enableInputDevices();
#endif

			if (server.featureWorkerManager().isWorkerRunning(message.featureUid()))
			{
				server.featureWorkerManager().sendMessageToManagedSystemWorker(message);
			}

			return true;
		}

		if (message.command<FeatureCommand>() == FeatureCommand::StartLock)
		{
#ifdef Q_OS_WIN
			PersistentScreenLockState::setLocked(message.featureUid());
#endif
		}

#ifndef Q_OS_WIN
		if( VeyonCore::platform().sessionFunctions().currentSessionHasUser() == false )
		{
			vDebug() << "not locking screen since not running in a user session";
			return true;
		}
#endif

		// Start the lock worker first so the student screen changes immediately.
		// HID/powercfg/PnP disable can take many seconds and runs in the background.
		server.featureWorkerManager().sendMessageToManagedSystemWorker( message );

#ifdef Q_OS_WIN
		if (message.command<FeatureCommand>() == FeatureCommand::StartLock)
		{
			VeyonCore::platform().inputDeviceFunctions().disableInputDevices();
		}
#endif

		return true;
	}

	return false;
}



bool ScreenLockFeaturePlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	if( message.featureUid() == m_screenLockFeature.uid() ||
		message.featureUid() == m_lockInputDevicesFeature.uid() )
	{
		switch (message.command<FeatureCommand>())
		{
		case FeatureCommand::StartLock:
			if( m_lockWidget == nullptr )
			{
				VeyonCore::platform().coreFunctions().disableScreenSaver();

				auto mode = LockWidget::BackgroundPixmap;
				if( message.featureUid() == m_lockInputDevicesFeature.uid() )
				{
					mode = LockWidget::DesktopVisible;
				}

				m_lockWidget = new LockWidget( mode,
											   QPixmap( QStringLiteral(":/screenlock/locked-screen-background.png" ) ) );
				connect(m_lockWidget, &LockWidget::failsafeUnlocked, this,
						[this, &worker, featureUid = message.featureUid()]() {
					worker.sendFeatureMessageReply(FeatureMessage{featureUid, FeatureCommand::FailsafeUnlock});
					if (m_lockWidget)
					{
						m_lockWidget->deleteLater();
						m_lockWidget = nullptr;
					}
					VeyonCore::platform().coreFunctions().restoreScreenSaverSettings();
					QTimer::singleShot(0, []() { QCoreApplication::quit(); });
				}, Qt::QueuedConnection);
			}
			return true;

		case FeatureCommand::StopLock:
			delete m_lockWidget;
			m_lockWidget = nullptr;

			VeyonCore::platform().coreFunctions().restoreScreenSaverSettings();

			QCoreApplication::quit();

			return true;

		case FeatureCommand::FailsafeUnlock:
			break;
		}
	}

	return false;
}



bool ScreenLockFeaturePlugin::handleFeatureMessageFromWorker(VeyonServerInterface& server, const FeatureMessage& message)
{
	Q_UNUSED(server)

	if ((message.featureUid() == m_screenLockFeature.uid() ||
		 message.featureUid() == m_lockInputDevicesFeature.uid()) &&
		message.command<FeatureCommand>() == FeatureCommand::FailsafeUnlock)
	{
#ifdef Q_OS_WIN
		FailsafePasswordState::clearPersistedInputLocks();
		VeyonCore::platform().inputDeviceFunctions().enableInputDevices();
#endif
		return true;
	}

	return false;
}



bool ScreenLockFeaturePlugin::isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const
{
	Q_UNUSED(server)

#ifdef Q_OS_WIN
	return PersistentScreenLockState::lockedFeatureUid() == featureUid;
#else
	Q_UNUSED(featureUid)
	return false;
#endif
}



void ScreenLockFeaturePlugin::initializeServer(VeyonServerInterface& server)
{
#ifdef Q_OS_WIN
	m_server = &server;
	restorePersistedLock();
#else
	Q_UNUSED(server)
#endif
}



#ifdef Q_OS_WIN
void ScreenLockFeaturePlugin::restorePersistedLock()
{
	if (m_server == nullptr)
	{
		return;
	}

	const auto featureUid = PersistentScreenLockState::lockedFeatureUid();
	if (featureUid.isNull())
	{
		return;
	}

	// Windows service starts veyon-server in the WTS session (via winlogon).
	// Disable input immediately, then restore the overlay as soon as that
	// session server is up, without waiting for an interactive user.
	VeyonCore::platform().inputDeviceFunctions().disableInputDevices();
	startLockWorker(*m_server, featureUid);

	if (m_server->featureWorkerManager().isWorkerRunning(featureUid) == false)
	{
		vDebug() << "persisted screen lock worker not running yet - retrying";
		QTimer::singleShot(RestoreLockRetryInterval, this, &ScreenLockFeaturePlugin::restorePersistedLock);
	}
}
#endif



#ifdef Q_OS_WIN
void ScreenLockFeaturePlugin::startLockWorker(VeyonServerInterface& server, Feature::Uid featureUid)
{
	if (server.featureWorkerManager().isWorkerRunning(featureUid))
	{
		return;
	}

	vInfo() << "restoring persisted screen lock" << featureUid;
	server.featureWorkerManager().sendMessageToManagedSystemWorker(
				FeatureMessage{featureUid, FeatureCommand::StartLock});
}
#endif
