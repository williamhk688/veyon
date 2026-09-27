/*
 * WebFilterSessionWatchdog.cpp - student-side monotonic expiry watchdog
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <algorithm>

#include <QDateTime>

#include "FailsafePasswordState.h"
#include "PersistentWebFilterState.h"
#include "PlatformInputDeviceFunctions.h"
#include "PlatformPluginInterface.h"
#include "VeyonCore.h"
#include "WebFilterEngine.h"
#include "WebFilterSessionPolicy.h"
#include "WebFilterSessionWatchdog.h"

WebFilterSessionWatchdog::WebFilterSessionWatchdog(QObject* parent) :
	QObject(parent)
{
	m_timer.setInterval(int(WebFilterSessionPolicy::WatchdogIntervalMs));
	connect(&m_timer, &QTimer::timeout, this, &WebFilterSessionWatchdog::tick);
}

void WebFilterSessionWatchdog::start()
{
	m_timer.start();
	tick();
}

void WebFilterSessionWatchdog::track(const WebFilterSession& session)
{
	if (session.sessionId == m_trackedSessionId)
	{
		return;
	}

	m_trackedSessionId = session.sessionId;
	m_baseElapsedMs = std::max<qint64>(0, session.checkpointElapsedMs);
	m_elapsed.restart();
	m_lastCheckpointMs = 0;
}

void WebFilterSessionWatchdog::stopCurrent(WebFilterSession::StopReason reason)
{
	const auto sessionId = m_trackedSessionId;
	if (WebFilterSessionPolicy::shouldFireTimer(sessionId, PersistentWebFilterState::session().sessionId) == false &&
		reason != WebFilterSession::StopReason::EmergencyUnlock)
	{
		return;
	}

	WebFilterEngine::stopSession(sessionId, reason, false);
	m_trackedSessionId = QUuid();
	m_baseElapsedMs = 0;
}

void WebFilterSessionWatchdog::tick()
{
	if (FailsafePasswordState::consumeEmergencyUnlockSucceeded())
	{
		FailsafePasswordState::clearPersistedInputLocks();
		VeyonCore::platform().inputDeviceFunctions().enableInputDevices();
		const auto session = PersistentWebFilterState::session();
		if (session.isActive())
		{
			m_trackedSessionId = session.sessionId;
			stopCurrent(WebFilterSession::StopReason::EmergencyUnlock);
		}
		return;
	}

	const auto session = PersistentWebFilterState::session();
	if (session.isActive() == false)
	{
		m_trackedSessionId = QUuid();
		m_baseElapsedMs = 0;
		return;
	}

	track(session);

	const auto elapsed = m_baseElapsedMs + m_elapsed.elapsed();
	if (WebFilterSessionPolicy::shouldFireTimer(m_trackedSessionId, PersistentWebFilterState::session().sessionId) == false)
	{
		return;
	}

	const auto action = WebFilterSessionPolicy::liveAction(session, elapsed, WebFilterEngine::configuredMaxTtlMs());
	if (action == WebFilterSessionPolicy::LiveAction::ExpireHardTtl)
	{
		stopCurrent(WebFilterSession::StopReason::HardTTLExpired);
		return;
	}
	if (action == WebFilterSessionPolicy::LiveAction::ExpireDuration)
	{
		stopCurrent(WebFilterSession::StopReason::ClientTimerExpired);
		return;
	}

	if (elapsed - m_lastCheckpointMs >= WebFilterSessionPolicy::CheckpointIntervalMs)
	{
		PersistentWebFilterState::updateCheckpoint(elapsed, QDateTime::currentMSecsSinceEpoch());
		m_lastCheckpointMs = elapsed;
	}
}
