/*
 * WindowsInputDeviceFunctions.cpp - implementation of WindowsInputDeviceFunctions class
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

#include <windows.h>

#include <cstring>

#include <QCoreApplication>
#include <QMutexLocker>
#include <QObject>
#include <QProcess>
#include <QThread>

#include "ConfigurationManager.h"
#include "FailsafePasswordKeyFilter.h"
#include "FailsafeUnlock.h"
#include "Logger.h"
#include "PlatformServiceFunctions.h"
#include "ProcessHelper.h"
#include "VeyonConfiguration.h"
#include "WindowsCoreFunctions.h"
#include "WindowsDeviceFunctions.h"
#include "WindowsInputDeviceFunctions.h"
#include "WindowsKeyboardShortcutTrapper.h"
#include "WindowsPlatformConfiguration.h"
#include "WtsSessionManager.h"


class Powercfg : public ProcessHelper
{
public:
	Powercfg(const QStringList& arguments) : ProcessHelper(QStringLiteral("powercfg"), arguments)
	{
	}
};


static int interception_is_any( InterceptionDevice device )
{
	Q_UNUSED(device)

	return true;
}


static WindowsDeviceFunctions::DeviceList inputDevicesToDisable()
{
	WindowsPlatformConfiguration config(&VeyonCore::config());

	WindowsDeviceFunctions::DeviceList devices;

	if (config.disableKeyboardDevicesForScreenLock())
	{
		devices += WindowsDeviceFunctions::findKeyboardDevices();
	}

	if (config.disableMouseDevicesForScreenLock())
	{
		devices += WindowsDeviceFunctions::findMouseDevices();
	}

	if (config.disableTouchDevicesForScreenLock())
	{
		devices += WindowsDeviceFunctions::findTouchDevices();
	}

	return devices;
}



WindowsInputDeviceFunctions::~WindowsInputDeviceFunctions()
{
	if( m_inputDevicesDisabled )
	{
		enableInputDevices();
	}
}



void WindowsInputDeviceFunctions::enableInputDevices()
{
	const auto generation = m_inputDisableGeneration.fetchAndAddOrdered(1) + 1;
	Q_UNUSED(generation)

	m_inputDevicesDisabled = false;

	disableInterception();
	restoreHIDService();
	restorePowerScheme();

	WindowsDeviceFunctions::DeviceList devicesToEnable;
	{
		QMutexLocker locker(&m_inputDeviceMutex);
		if (m_disabledInputDevices.isEmpty() == false)
		{
			devicesToEnable = m_disabledInputDevices;
			m_disabledInputDevices.clear();
		}
	}

	if (devicesToEnable.isEmpty())
	{
		devicesToEnable = inputDevicesToDisable();
	}

	WindowsDeviceFunctions::setDevicesState(devicesToEnable, WindowsDeviceFunctions::State::Enabled);
}



void WindowsInputDeviceFunctions::disableInputDevices()
{
	if (m_inputDevicesDisabled)
	{
		return;
	}

	// Interception blocks keys immediately. HID/powercfg/PnP device
	// disable can take 10–15s and must not delay the lock/demo worker.
	enableInterception();
	m_inputDevicesDisabled = true;
	const auto generation = m_inputDisableGeneration.loadAcquire();

	auto* thread = QThread::create([this, generation]() {
		finishDisablingInputDevices(generation);
	});
	QObject::connect(thread, &QThread::finished, thread, &QThread::deleteLater);
	thread->start();
}



void WindowsInputDeviceFunctions::finishDisablingInputDevices(int generation)
{
	if (m_inputDevicesDisabled == false ||
		m_inputDisableGeneration.loadAcquire() != generation)
	{
		return;
	}

	if (m_interceptionContext == nullptr)
	{
		stopHIDService();
	}

	if (m_inputDevicesDisabled == false ||
		m_inputDisableGeneration.loadAcquire() != generation)
	{
		return;
	}

	setCustomPowerScheme();

	if (m_inputDevicesDisabled == false ||
		m_inputDisableGeneration.loadAcquire() != generation)
	{
		return;
	}

	auto devices = inputDevicesToDisable();
	if (m_interceptionContext == nullptr)
	{
		vWarning() << "Interception driver is not available; falling back to disabling keyboard and mouse devices";
		devices += WindowsDeviceFunctions::findKeyboardDevices();
		devices += WindowsDeviceFunctions::findMouseDevices();
	}

	if (m_inputDevicesDisabled == false ||
		m_inputDisableGeneration.loadAcquire() != generation)
	{
		return;
	}

	{
		QMutexLocker locker(&m_inputDeviceMutex);
		if (m_inputDevicesDisabled == false ||
			m_inputDisableGeneration.loadAcquire() != generation)
		{
			return;
		}
		m_disabledInputDevices = devices;
	}

	if (devices.isEmpty() == false)
	{
		WindowsDeviceFunctions::setDevicesState(devices, WindowsDeviceFunctions::State::Disabled);
		if (m_inputDevicesDisabled == false ||
			m_inputDisableGeneration.loadAcquire() != generation)
		{
			WindowsDeviceFunctions::setDevicesState(devices, WindowsDeviceFunctions::State::Enabled);
		}
	}
}



KeyboardShortcutTrapper* WindowsInputDeviceFunctions::createKeyboardShortcutTrapper( QObject* parent )
{
	return new WindowsKeyboardShortcutTrapper( parent );
}



void WindowsInputDeviceFunctions::checkInterceptionInstallation()
{
	if( VeyonCore::config().multiSessionModeEnabled() )
	{
		uninstallInterception();
	}
	else if( WindowsPlatformConfiguration( &VeyonCore::config() ).useInterceptionDriver() )
	{
		const auto context = interception_create_context();
		if( context )
		{
			// a valid context means the interception driver is installed properly
			// so nothing to do here
			interception_destroy_context( context );
		}
		// try to (re)install interception driver
		else if( installInterception() == false )
		{
			// failed to uninstall it so we can try to install it again on next reboot
			uninstallInterception();
		}
	}
}



void WindowsInputDeviceFunctions::stopOnScreenKeyboard()
{
	WindowsCoreFunctions::terminateProcess( WtsSessionManager::findProcessId( QStringLiteral("osk.exe") ) );
	WindowsCoreFunctions::terminateProcess( WtsSessionManager::findProcessId( QStringLiteral("tabtip.exe") ) );
}



void WindowsInputDeviceFunctions::enableInterception()
{
	if( WindowsPlatformConfiguration( &VeyonCore::config() ).useInterceptionDriver() )
	{
		FailsafeHotkeyMonitor::instance();

		m_interceptionContext = interception_create_context();

		if( m_interceptionContext )
		{
			interception_set_filter(m_interceptionContext,
									interception_is_any,
									InterceptionFilter(INTERCEPTION_FILTER_KEY_ALL) | InterceptionFilter(INTERCEPTION_FILTER_MOUSE_ALL));
			startInterceptionReceiveThread();
		}
		else
		{
			vCritical() << "failed to create interception context";
		}
	}
}



void WindowsInputDeviceFunctions::disableInterception()
{
	stopInterceptionReceiveThread();

	if( m_interceptionContext )
	{
		interception_destroy_context( m_interceptionContext );
		m_interceptionContext = nullptr;
	}
}



void WindowsInputDeviceFunctions::startInterceptionReceiveThread()
{
	if (m_interceptionReceiveThread)
	{
		m_interceptionReceiveThread->requestInterruption();
		m_interceptionReceiveThread->wait();
		delete m_interceptionReceiveThread;
		m_interceptionReceiveThread = nullptr;
	}

	if (m_interceptionContext == nullptr)
	{
		return;
	}

	m_interceptionReceiveThread = QThread::create([this]() {
		runInterceptionReceiveLoop();
	});
	m_interceptionReceiveThread->start();
}



void WindowsInputDeviceFunctions::stopInterceptionReceiveThread()
{
	if (m_interceptionReceiveThread == nullptr)
	{
		return;
	}

	m_interceptionReceiveThread->requestInterruption();
	m_interceptionReceiveThread->wait();
	delete m_interceptionReceiveThread;
	m_interceptionReceiveThread = nullptr;
}



void WindowsInputDeviceFunctions::runInterceptionReceiveLoop()
{
	static constexpr unsigned short ScanControl = 0x1D;
	static constexpr unsigned short ScanAlt = 0x38;
	static constexpr unsigned short ScanLeftShift = 0x2A;
	static constexpr unsigned short ScanRightShift = 0x36;
	static constexpr unsigned short ScanU = 0x16;

	bool controlDown = false;
	bool altDown = false;
	bool shiftDown = false;
	bool hotkeyArmed = true;

	bool leftShiftDown = false;
	bool rightShiftDown = false;
	bool shiftInjected = false;
	InterceptionDevice lastKeyboardDevice = 0;
	InterceptionKeyStroke injectedShiftStroke{};

	const auto sendKeyStroke = [this](InterceptionDevice device, const InterceptionKeyStroke& keyStroke) {
		InterceptionStroke outbound{};
		memcpy(&outbound, &keyStroke, sizeof(keyStroke));
		interception_send(m_interceptionContext, device, &outbound, 1);
	};

	const auto sendShiftUpIfInjected = [&]() {
		if (shiftInjected == false || lastKeyboardDevice == 0 || m_interceptionContext == nullptr)
		{
			shiftInjected = false;
			return;
		}

		InterceptionKeyStroke shiftUp = injectedShiftStroke;
		shiftUp.state = InterceptionKeyState(shiftUp.state | INTERCEPTION_KEY_UP);
		sendKeyStroke(lastKeyboardDevice, shiftUp);
		shiftInjected = false;
	};

	while (m_interceptionReceiveThread &&
		   m_interceptionReceiveThread->isInterruptionRequested() == false &&
		   m_interceptionContext)
	{
		const auto device = interception_wait_with_timeout(m_interceptionContext, 50);
		if (interception_is_invalid(device))
		{
			if (FailsafeHotkeyMonitor::instance().passwordPromptActive() == false)
			{
				sendShiftUpIfInjected();
			}
			continue;
		}

		InterceptionStroke stroke{};
		if (interception_receive(m_interceptionContext, device, &stroke, 1) <= 0)
		{
			break;
		}

		const bool promptActive = FailsafeHotkeyMonitor::instance().passwordPromptActive();
		if (promptActive == false)
		{
			sendShiftUpIfInjected();
		}

		if (promptActive)
		{
			if (interception_is_mouse(device))
			{
				interception_send(m_interceptionContext, device, &stroke, 1);
				continue;
			}

			if (interception_is_keyboard(device) == false)
			{
				continue;
			}

			const auto* keyStroke = reinterpret_cast<const InterceptionKeyStroke *>(&stroke);
			const bool extended = (keyStroke->state & INTERCEPTION_KEY_E0) != 0;
			const bool e1 = (keyStroke->state & INTERCEPTION_KEY_E1) != 0;
			const bool isUp = (keyStroke->state & INTERCEPTION_KEY_UP) != 0;
			lastKeyboardDevice = device;

			if (FailsafePasswordKeyFilter::isShiftKey(keyStroke->code, extended))
			{
				if (keyStroke->code == ScanLeftShift)
				{
					leftShiftDown = (isUp == false);
				}
				else
				{
					rightShiftDown = (isUp == false);
				}

				if (isUp == false)
				{
					injectedShiftStroke = *keyStroke;
					injectedShiftStroke.state = InterceptionKeyState(keyStroke->state & ~INTERCEPTION_KEY_UP);
				}
				else if (leftShiftDown == false && rightShiftDown == false)
				{
					sendShiftUpIfInjected();
				}

				// Isolated Shift is swallowed so Sticky Keys / Filter Keys
				// never see five taps or an eight-second hold.
				continue;
			}

			if (FailsafePasswordKeyFilter::isAllowedTypingKey(keyStroke->code, extended, e1))
			{
				if ((leftShiftDown || rightShiftDown) && shiftInjected == false)
				{
					sendKeyStroke(device, injectedShiftStroke);
					shiftInjected = true;
				}
				interception_send(m_interceptionContext, device, &stroke, 1);
			}

			continue;
		}

		if (interception_is_keyboard(device) == false)
		{
			continue;
		}

		const auto* keyStroke = reinterpret_cast<const InterceptionKeyStroke *>(&stroke);
		const bool isUp = (keyStroke->state & INTERCEPTION_KEY_UP) != 0;

		switch (keyStroke->code)
		{
		case ScanControl:
			controlDown = (isUp == false);
			break;
		case ScanAlt:
			altDown = (isUp == false);
			break;
		case ScanLeftShift:
		case ScanRightShift:
			shiftDown = (isUp == false);
			break;
		default:
			break;
		}

		if (keyStroke->code == ScanU)
		{
			if (isUp)
			{
				hotkeyArmed = true;
			}
			else if (hotkeyArmed && controlDown && altDown && shiftDown)
			{
				hotkeyArmed = false;
				FailsafeHotkeyMonitor::instance().notifyHotkeyPressed();
			}
		}
	}

	sendShiftUpIfInjected();
}



void WindowsInputDeviceFunctions::initHIDServiceStatus()
{
	if( m_hidServiceStatusInitialized == false )
	{
		m_hidServiceActivated = VeyonCore::platform().serviceFunctions().isRunning( m_hidServiceName );
		m_hidServiceStatusInitialized = true;
	}
}



void WindowsInputDeviceFunctions::restoreHIDService()
{
	if( m_hidServiceActivated )
	{
		VeyonCore::platform().serviceFunctions().start( m_hidServiceName );
	}
}



void WindowsInputDeviceFunctions::setCustomPowerScheme()
{
	if (WindowsPlatformConfiguration(&VeyonCore::config()).useCustomPowerSchemeForScreenLock() == false ||
		m_originalPowerSchemeId.isEmpty() == false)
	{
		return;
	}

	QUuid activeSchemeId{Powercfg{{QStringLiteral("/getactivescheme")}}.runAndReadAll().split(':').value(1).trimmed().split(' ').constFirst()};
	if (activeSchemeId.isNull() || activeSchemeId == CustomPowerSchemeId)
	{
		return;
	}

	m_originalPowerSchemeId = activeSchemeId.toString(QUuid::WithoutBraces);
	Powercfg{{QStringLiteral("/delete"), CustomPowerSchemeIdString}}.run();
	Powercfg{{QStringLiteral("/duplicatescheme"), m_originalPowerSchemeId, CustomPowerSchemeIdString}}.run();
	Powercfg{{QStringLiteral("/setacvalueindex"), CustomPowerSchemeIdString,
					QStringLiteral("4f971e89-eebd-4455-a8de-9e59040e7347"), QStringLiteral("7648efa3-dd9c-4e3e-b566-50f929386280"), QStringLiteral("0")}}.run();
	Powercfg{{QStringLiteral("/setdcvalueindex"), CustomPowerSchemeIdString,
					QStringLiteral("4f971e89-eebd-4455-a8de-9e59040e7347"), QStringLiteral("7648efa3-dd9c-4e3e-b566-50f929386280"), QStringLiteral("0")}}.run();
	Powercfg{{QStringLiteral("/setacvalueindex"), CustomPowerSchemeIdString,
					QStringLiteral("4f971e89-eebd-4455-a8de-9e59040e7347"), QStringLiteral("96996bc0-ad50-47ec-923b-6f41874dd9eb"), QStringLiteral("0")}}.run();
	Powercfg{{QStringLiteral("/setdcvalueindex"), CustomPowerSchemeIdString,
					QStringLiteral("4f971e89-eebd-4455-a8de-9e59040e7347"), QStringLiteral("96996bc0-ad50-47ec-923b-6f41874dd9eb"), QStringLiteral("0")}}.run();
	Powercfg{{QStringLiteral("/setactive"), CustomPowerSchemeIdString}}.run();
}



void WindowsInputDeviceFunctions::restorePowerScheme()
{
	if (m_originalPowerSchemeId.isNull() == false)
	{
		Powercfg{{QStringLiteral("/setactive"), m_originalPowerSchemeId}}.run();
		Powercfg{{QStringLiteral("/delete"), CustomPowerSchemeIdString}}.run();
	}
}



void WindowsInputDeviceFunctions::stopHIDService()
{
	initHIDServiceStatus();

	if( m_hidServiceActivated )
	{
		VeyonCore::platform().serviceFunctions().stop( m_hidServiceName );
	}
}



bool WindowsInputDeviceFunctions::installInterception()
{
	return interceptionInstaller( QStringLiteral("/install") ) == 0;
}



bool WindowsInputDeviceFunctions::uninstallInterception()
{
	return interceptionInstaller( QStringLiteral("/uninstall") ) == 0;
}



int WindowsInputDeviceFunctions::interceptionInstaller( const QString& argument )
{
	return QProcess::execute( QCoreApplication::applicationDirPath() + QStringLiteral("/interception/install-interception.exe"), { argument } );
}
