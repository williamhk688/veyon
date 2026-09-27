/*
 * WebFilterSessionPolicy.cpp - expiry, stale-message, and recovery rules
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "WebFilterSessionPolicy.h"

qint64 WebFilterSessionPolicy::clampDurationMs(qint64 requestedMs, qint64 maxTtlMs)
{
	const auto maxMs = maxTtlMs > 0 ? maxTtlMs : DefaultMaxTtlMs;
	if (requestedMs < MinDurationMs)
	{
		return MinDurationMs;
	}
	if (requestedMs > maxMs)
	{
		return maxMs;
	}
	return requestedMs;
}

qint64 WebFilterSessionPolicy::durationUntilEndMs(qint64 startMs, qint64 endMs, qint64 maxTtlMs)
{
	return clampDurationMs(endMs - startMs, maxTtlMs);
}

WebFilterSession WebFilterSessionPolicy::create(WebFilterSession::Mode mode,
												qint64 durationMs,
												qint64 startWallMs,
												qint64 maxTtlMs,
												const QStringList& domains)
{
	WebFilterSession session;
	session.sessionId = QUuid::createUuid();
	session.mode = mode;
	session.startTimeMs = startWallMs;
	session.durationMs = clampDurationMs(durationMs, maxTtlMs);
	session.expiresAtMs = startWallMs + session.durationMs;
	session.hardExpiresAtMs = startWallMs + (maxTtlMs > 0 ? maxTtlMs : DefaultMaxTtlMs);
	session.version = CurrentVersion;
	session.domains = domains;
	session.checkpointElapsedMs = 0;
	session.checkpointWallMs = startWallMs;
	session.checkpointTickMs = currentUptimeMs();
	return session;
}

bool WebFilterSessionPolicy::isWellFormed(const WebFilterSession& session,
										  qint64 maxTtlMs,
										  QString* reason)
{
	const auto fail = [reason](const char* text) {
		if (reason)
		{
			*reason = QString::fromLatin1(text);
		}
		return false;
	};

	if (session.sessionId.isNull())
	{
		return fail("sessionId missing");
	}
	if (session.mode != WebFilterSession::Mode::Blacklist &&
		session.mode != WebFilterSession::Mode::Whitelist)
	{
		return fail("mode missing");
	}
	if (session.version != CurrentVersion)
	{
		return fail("unsupported persisted version");
	}
	if (session.durationMs <= 0)
	{
		return fail("duration illegal");
	}
	const auto maxMs = maxTtlMs > 0 ? maxTtlMs : DefaultMaxTtlMs;
	if (session.durationMs > maxMs)
	{
		return fail("duration exceeds hard TTL");
	}
	if (session.startTimeMs <= 0)
	{
		return fail("startTime missing");
	}
	if (session.expiresAtMs <= session.startTimeMs)
	{
		return fail("expiresAt unreasonable");
	}
	if (session.checkpointElapsedMs < 0)
	{
		return fail("checkpoint elapsed illegal");
	}
	return true;
}

bool WebFilterSessionPolicy::isStaleSession(const QUuid& incoming, const QUuid& current)
{
	return incoming.isNull() == false &&
			current.isNull() == false &&
			incoming != current;
}

bool WebFilterSessionPolicy::shouldApplyUnlock(const QUuid& incoming, const QUuid& current)
{
	if (current.isNull())
	{
		return true;
	}
	if (incoming.isNull())
	{
		return true;
	}
	return incoming == current;
}

bool WebFilterSessionPolicy::shouldFireTimer(const QUuid& timerId, const QUuid& current)
{
	return timerId.isNull() == false && timerId == current;
}

WebFilterSessionPolicy::LiveAction WebFilterSessionPolicy::liveAction(const WebFilterSession& session,
																	  qint64 monotonicElapsedMs,
																	  qint64 maxTtlMs)
{
	if (session.isActive() == false)
	{
		return LiveAction::Continue;
	}

	const auto maxMs = maxTtlMs > 0 ? maxTtlMs : DefaultMaxTtlMs;
	if (monotonicElapsedMs >= maxMs)
	{
		return LiveAction::ExpireHardTtl;
	}
	if (monotonicElapsedMs >= session.durationMs)
	{
		return LiveAction::ExpireDuration;
	}
	return LiveAction::Continue;
}

qint64 WebFilterSessionPolicy::recoveredElapsedMs(const WebFilterSession& session,
												  qint64 nowWallMs,
												  qint64 maxTtlMs)
{
	const auto lastWall = session.checkpointWallMs > 0 ? session.checkpointWallMs : session.startTimeMs;
	const auto rawDelta = nowWallMs - lastWall;
	const auto maxMs = maxTtlMs > 0 ? maxTtlMs : DefaultMaxTtlMs;
	if (rawDelta < -ClockBackwardSlackMs)
	{
		return std::max<qint64>(0, session.checkpointElapsedMs);
	}

	const auto wallDelta = std::clamp<qint64>(rawDelta, 0, maxMs);
	return std::max<qint64>(0, session.checkpointElapsedMs) + wallDelta;
}

qint64 WebFilterSessionPolicy::currentUptimeMs()
{
#ifdef Q_OS_WIN
	return qint64(GetTickCount64());
#else
	return 0;
#endif
}

bool WebFilterSessionPolicy::isSameBoot(const WebFilterSession& session, qint64 nowTickMs)
{
	const auto now = nowTickMs >= 0 ? nowTickMs : currentUptimeMs();
	return session.checkpointTickMs > 0 && now >= session.checkpointTickMs;
}

WebFilterSessionPolicy::RecoveryAction WebFilterSessionPolicy::recoveryAction(const WebFilterSession& session,
																			  qint64 nowWallMs,
																			  qint64 maxTtlMs,
																			  QString* reason)
{
	if (isWellFormed(session, maxTtlMs, reason) == false)
	{
		return RecoveryAction::FailOpen;
	}

	const auto elapsed = recoveredElapsedMs(session, nowWallMs, maxTtlMs);
	const auto maxMs = maxTtlMs > 0 ? maxTtlMs : DefaultMaxTtlMs;
	if (elapsed >= maxMs)
	{
		if (reason)
		{
			*reason = QStringLiteral("hard TTL exceeded");
		}
		return RecoveryAction::Expire;
	}
	if (elapsed >= session.durationMs)
	{
		if (reason)
		{
			*reason = QStringLiteral("session expired");
		}
		return RecoveryAction::Expire;
	}

	if (reason)
	{
		*reason = QStringLiteral("session still valid");
	}
	return RecoveryAction::Reapply;
}

WebFilterSession WebFilterSessionPolicy::recoverForReboot(const WebFilterSession& loaded,
														  qint64 nowWallMs,
														  qint64 maxTtlMs,
														  QString* reason)
{
	if (isWellFormed(loaded, maxTtlMs, reason))
	{
		return loaded;
	}

	if (loaded.mode != WebFilterSession::Mode::Blacklist &&
		loaded.mode != WebFilterSession::Mode::Whitelist)
	{
		return loaded;
	}

	if (loaded.startTimeMs > 0 && loaded.durationMs > 0)
	{
		auto repaired = loaded;
		if (repaired.sessionId.isNull())
		{
			repaired.sessionId = QUuid::createUuid();
		}
		if (repaired.version != CurrentVersion)
		{
			repaired.version = CurrentVersion;
		}
		if (repaired.expiresAtMs <= repaired.startTimeMs)
		{
			repaired.expiresAtMs = repaired.startTimeMs + repaired.durationMs;
		}
		const auto maxMs = maxTtlMs > 0 ? maxTtlMs : DefaultMaxTtlMs;
		if (repaired.hardExpiresAtMs <= repaired.startTimeMs)
		{
			repaired.hardExpiresAtMs = repaired.startTimeMs + maxMs;
		}
		if (reason)
		{
			*reason = QStringLiteral("repaired persisted timestamps");
		}
		return repaired;
	}

	const auto reconstructed = create(loaded.mode,
									  maxTtlMs > 0 ? maxTtlMs : DefaultMaxTtlMs,
									  nowWallMs,
									  maxTtlMs,
									  loaded.domains);
	if (reason)
	{
		*reason = QStringLiteral("reconstructed persisted mode");
	}
	return reconstructed;
}

qint64 WebFilterSessionPolicy::remainingMs(qint64 durationMs, qint64 elapsedMs, qint64 maxTtlMs)
{
	const auto limit = std::min(durationMs, maxTtlMs > 0 ? maxTtlMs : DefaultMaxTtlMs);
	return std::max<qint64>(0, limit - std::max<qint64>(0, elapsedMs));
}

int WebFilterSessionPolicy::authFailureDelayMs(int failureCount)
{
	if (failureCount <= 0)
	{
		return 0;
	}
	return std::min(failureCount * AuthFailureBaseDelayMs, AuthFailureMaxDelayMs);
}

QString WebFilterSessionPolicy::reasonName(WebFilterSession::StopReason reason)
{
	switch (reason)
	{
	case WebFilterSession::StopReason::TeacherManual:
		return QStringLiteral("TeacherManual");
	case WebFilterSession::StopReason::TeacherTimerExpired:
		return QStringLiteral("TeacherTimerExpired");
	case WebFilterSession::StopReason::ClientTimerExpired:
		return QStringLiteral("ClientTimerExpired");
	case WebFilterSession::StopReason::RecoveryExpired:
		return QStringLiteral("RecoveryExpired");
	case WebFilterSession::StopReason::HardTTLExpired:
		return QStringLiteral("HardTTLExpired");
	case WebFilterSession::StopReason::EmergencyUnlock:
		return QStringLiteral("EmergencyUnlock");
	}
	return QStringLiteral("Unknown");
}

QString WebFilterSessionPolicy::modeName(WebFilterSession::Mode mode)
{
	switch (mode)
	{
	case WebFilterSession::Mode::Blacklist:
		return QStringLiteral("Blacklist");
	case WebFilterSession::Mode::Whitelist:
		return QStringLiteral("Whitelist");
	case WebFilterSession::Mode::Off:
		break;
	}
	return QStringLiteral("None");
}
