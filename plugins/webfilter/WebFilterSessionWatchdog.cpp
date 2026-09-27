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
	QThread(parent)
{
}

void WebFilterSessionWatchdog::run()
{
	while (isInterruptionRequested() == false)
	{
		poll();
		for (int slept = 0; slept < WebFilterSessionPolicy::WatchdogIntervalMs &&
			 isInterruptionRequested() == false; slept += 50)
		{
			QThread::msleep(50);
		}
	}
}

void WebFilterSessionWatchdog::track(const WebFilterSession& session)
{
	if (session.sessionId == m_trackedSessionId)
	{
		const auto current = m_baseElapsedMs + m_elapsed.elapsed();
		if (session.checkpointElapsedMs > current)
		{
			m_baseElapsedMs = session.checkpointElapsedMs;
			m_elapsed.restart();
		}
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

bool WebFilterSessionWatchdog::poll()
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
		return true;
	}

	const auto session = PersistentWebFilterState::session();
	if (session.isActive() == false)
	{
		m_trackedSessionId = QUuid();
		m_baseElapsedMs = 0;
		return false;
	}

	track(session);

	const auto maxTtlMs = WebFilterEngine::configuredMaxTtlMs();
	const auto nowWall = QDateTime::currentMSecsSinceEpoch();
	if (WebFilterSessionPolicy::isSameBoot(session) == false)
	{
		if (WebFilterSessionPolicy::recoveryAction(session, nowWall, maxTtlMs)
			== WebFilterSessionPolicy::RecoveryAction::Expire)
		{
			stopCurrent(WebFilterSession::StopReason::RecoveryExpired);
			return true;
		}

		const auto recovered = WebFilterSessionPolicy::recoveredElapsedMs(session, nowWall, maxTtlMs);
		const auto current = m_baseElapsedMs + m_elapsed.elapsed();
		if (recovered > current)
		{
			m_baseElapsedMs = recovered;
			m_elapsed.restart();
			PersistentWebFilterState::updateCheckpoint(recovered, nowWall);
			m_lastCheckpointMs = recovered;
		}
	}

	const auto elapsed = m_baseElapsedMs + m_elapsed.elapsed();
	if (WebFilterSessionPolicy::shouldFireTimer(m_trackedSessionId, PersistentWebFilterState::session().sessionId) == false)
	{
		return false;
	}

	const auto action = WebFilterSessionPolicy::liveAction(session, elapsed, maxTtlMs);
	if (action == WebFilterSessionPolicy::LiveAction::ExpireHardTtl)
	{
		stopCurrent(WebFilterSession::StopReason::HardTTLExpired);
		return true;
	}
	if (action == WebFilterSessionPolicy::LiveAction::ExpireDuration)
	{
		stopCurrent(WebFilterSession::StopReason::ClientTimerExpired);
		return true;
	}

	if (elapsed - m_lastCheckpointMs >= WebFilterSessionPolicy::CheckpointIntervalMs)
	{
		PersistentWebFilterState::updateCheckpoint(elapsed, QDateTime::currentMSecsSinceEpoch());
		m_lastCheckpointMs = elapsed;
	}
	return false;
}
