/*
 * WebFilterSessionPolicy.h - expiry, stale-message, and recovery rules
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include "WebFilterSession.h"

class WebFilterSessionPolicy
{
public:
	static constexpr int CurrentVersion = 1;
	static constexpr qint64 DefaultMaxTtlMs = 3LL * 60 * 60 * 1000;
	static constexpr qint64 MinDurationMs = 60LL * 1000;
	static constexpr qint64 CheckpointIntervalMs = 15LL * 1000;
	static constexpr qint64 WatchdogIntervalMs = 1000;
	static constexpr qint64 ClockBackwardSlackMs = 5LL * 60 * 1000;
	static constexpr int AuthFailureBaseDelayMs = 500;
	static constexpr int AuthFailureMaxDelayMs = 3000;

	enum class RecoveryAction
	{
		FailOpen,
		Expire,
		Reapply
	};

	enum class LiveAction
	{
		Continue,
		ExpireDuration,
		ExpireHardTtl
	};

	static qint64 clampDurationMs(qint64 requestedMs, qint64 maxTtlMs = DefaultMaxTtlMs);
	static qint64 durationUntilEndMs(qint64 startMs, qint64 endMs, qint64 maxTtlMs = DefaultMaxTtlMs);
	static WebFilterSession create(WebFilterSession::Mode mode,
								   qint64 durationMs,
								   qint64 startWallMs,
								   qint64 maxTtlMs = DefaultMaxTtlMs,
								   const QStringList& domains = {});

	static bool isWellFormed(const WebFilterSession& session,
							 qint64 maxTtlMs = DefaultMaxTtlMs,
							 QString* reason = nullptr);
	static bool isStaleSession(const QUuid& incoming, const QUuid& current);
	static bool shouldApplyUnlock(const QUuid& incoming, const QUuid& current);
	static bool shouldFireTimer(const QUuid& timerId, const QUuid& current);

	static LiveAction liveAction(const WebFilterSession& session,
								 qint64 monotonicElapsedMs,
								 qint64 maxTtlMs = DefaultMaxTtlMs);
	static RecoveryAction recoveryAction(const WebFilterSession& session,
										 qint64 nowWallMs,
										 qint64 maxTtlMs = DefaultMaxTtlMs,
										 QString* reason = nullptr);
	static WebFilterSession recoverForReboot(const WebFilterSession& loaded,
											 qint64 nowWallMs,
											 qint64 maxTtlMs = DefaultMaxTtlMs,
											 QString* reason = nullptr);
	static qint64 recoveredElapsedMs(const WebFilterSession& session,
									 qint64 nowWallMs,
									 qint64 maxTtlMs = DefaultMaxTtlMs);
	static qint64 currentUptimeMs();
	static bool isSameBoot(const WebFilterSession& session, qint64 nowTickMs = -1);
	static qint64 remainingMs(qint64 durationMs, qint64 elapsedMs, qint64 maxTtlMs = DefaultMaxTtlMs);
	static int authFailureDelayMs(int failureCount);

	static QString reasonName(WebFilterSession::StopReason reason);
	static QString modeName(WebFilterSession::Mode mode);
};
