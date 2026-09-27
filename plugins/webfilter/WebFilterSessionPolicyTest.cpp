/*
 * WebFilterSessionPolicyTest.cpp - session lifecycle, recovery, and expiry
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 */

#include <QtTest>

#include "WebFilterLists.h"
#include "WebFilterSessionPolicy.h"

class WebFilterSessionPolicyTest : public QObject
{
	Q_OBJECT
private slots:
	void startBlacklistSession()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 30 * 60 * 1000, 1'000'000);
		QVERIFY(session.isActive());
		QCOMPARE(session.mode, WebFilterSession::Mode::Blacklist);
		QCOMPARE(session.durationMs, 30 * 60 * 1000);
		QCOMPARE(session.expiresAtMs, session.startTimeMs + session.durationMs);
	}

	void startWhitelistSession()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 45 * 60 * 1000, 1'000'000,
					WebFilterSessionPolicy::DefaultMaxTtlMs,
					{QStringLiteral("classroom.google.com")});
		QVERIFY(session.isActive());
		QCOMPARE(session.mode, WebFilterSession::Mode::Whitelist);
		QVERIFY(session.domains.contains(QStringLiteral("classroom.google.com")));
	}

	void teacherManualRestore()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 10 * 60 * 1000, 1'000'000);
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock(session.sessionId, session.sessionId));
		QVERIFY(session.isActive());
	}

	void teacherTimerExpiresSameSession()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 10 * 60 * 1000, 1'000'000);
		QVERIFY(WebFilterSessionPolicy::shouldFireTimer(session.sessionId, session.sessionId));
		QCOMPARE(WebFilterSessionPolicy::liveAction(session, session.durationMs),
				 WebFilterSessionPolicy::LiveAction::ExpireDuration);
	}

	void clientTimerExpires()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 20 * 60 * 1000, 1'000'000);
		QCOMPARE(WebFilterSessionPolicy::liveAction(session, session.durationMs - 1),
				 WebFilterSessionPolicy::LiveAction::Continue);
		QCOMPARE(WebFilterSessionPolicy::liveAction(session, session.durationMs),
				 WebFilterSessionPolicy::LiveAction::ExpireDuration);
	}

	void disconnectDoesNotExpire()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 30 * 60 * 1000, 1'000'000);
		QCOMPARE(WebFilterSessionPolicy::liveAction(session, 5 * 60 * 1000),
				 WebFilterSessionPolicy::LiveAction::Continue);
	}

	void disconnectThenLocalExpiry()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 10 * 60 * 1000, 1'000'000);
		QCOMPARE(WebFilterSessionPolicy::liveAction(session, 10 * 60 * 1000),
				 WebFilterSessionPolicy::LiveAction::ExpireDuration);
	}

	void teacherCrashDoesNotExpire()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 60 * 60 * 1000, 1'000'000);
		QCOMPARE(WebFilterSessionPolicy::liveAction(session, 1000),
				 WebFilterSessionPolicy::LiveAction::Continue);
	}

	void serviceRestartStillValid()
	{
		auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 30 * 60 * 1000, 1'000'000);
		session.checkpointElapsedMs = 5 * 60 * 1000;
		session.checkpointWallMs = session.startTimeMs + session.checkpointElapsedMs;
		QString reason;
		QCOMPARE(WebFilterSessionPolicy::recoveryAction(session, session.checkpointWallMs + 1000, {}, &reason),
				 WebFilterSessionPolicy::RecoveryAction::Reapply);
	}

	void rebootRestoresUnexpiredSession()
	{
		auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 45 * 60 * 1000, 2'000'000);
		session.checkpointElapsedMs = 10 * 60 * 1000;
		session.checkpointWallMs = session.startTimeMs + 10 * 60 * 1000;
		QCOMPARE(WebFilterSessionPolicy::recoveryAction(session, session.checkpointWallMs + 60 * 1000),
				 WebFilterSessionPolicy::RecoveryAction::Reapply);
	}

	void rebootFindsExpiredSession()
	{
		auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 10 * 60 * 1000, 3'000'000);
		session.checkpointElapsedMs = 9 * 60 * 1000;
		session.checkpointWallMs = session.startTimeMs + 9 * 60 * 1000;
		QCOMPARE(WebFilterSessionPolicy::recoveryAction(session, session.checkpointWallMs + 5 * 60 * 1000),
				 WebFilterSessionPolicy::RecoveryAction::Expire);
	}

	void liveWallClockForwardDoesNotExpire()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 30 * 60 * 1000, 1'000'000);
		QCOMPARE(WebFilterSessionPolicy::liveAction(session, 60 * 1000),
				 WebFilterSessionPolicy::LiveAction::Continue);
	}

	void liveWallClockBackwardStillExpiresOnMonotonic()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 10 * 60 * 1000, 1'000'000);
		QCOMPARE(WebFilterSessionPolicy::liveAction(session, session.durationMs),
				 WebFilterSessionPolicy::LiveAction::ExpireDuration);
	}

	void recoveryClockBackwardKeepsRestriction()
	{
		auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 30 * 60 * 1000, 5'000'000);
		session.checkpointElapsedMs = 2 * 60 * 1000;
		session.checkpointWallMs = session.startTimeMs + 2 * 60 * 1000;
		const auto now = session.checkpointWallMs - 24LL * 60 * 60 * 1000;
		QCOMPARE(WebFilterSessionPolicy::recoveryAction(session, now),
				 WebFilterSessionPolicy::RecoveryAction::Reapply);
		QCOMPARE(WebFilterSessionPolicy::recoveredElapsedMs(session, now), session.checkpointElapsedMs);
	}

	void hardTtlExpires()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 30 * 60 * 1000, 1'000'000);
		QCOMPARE(WebFilterSessionPolicy::liveAction(session, WebFilterSessionPolicy::DefaultMaxTtlMs),
				 WebFilterSessionPolicy::LiveAction::ExpireHardTtl);
	}

	void sessionATimerDoesNotStopSessionB()
	{
		const auto a = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 10 * 60 * 1000, 1'000'000);
		const auto b = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 20 * 60 * 1000, 1'000'100);
		QVERIFY(WebFilterSessionPolicy::isStaleSession(a.sessionId, b.sessionId));
		QVERIFY(WebFilterSessionPolicy::shouldFireTimer(a.sessionId, b.sessionId) == false);
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock(a.sessionId, b.sessionId) == false);
	}

	void delayedUnlockADoesNotStopB()
	{
		const auto a = QUuid::createUuid();
		const auto b = QUuid::createUuid();
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock(a, b) == false);
	}

	void duplicateUnlockIsIdempotent()
	{
		const auto id = QUuid::createUuid();
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock(id, id));
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock(id, {}));
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock({}, {}));
	}

	void duplicateTimeoutIgnoredForReplacedSession()
	{
		const auto a = QUuid::createUuid();
		const auto b = QUuid::createUuid();
		QVERIFY(WebFilterSessionPolicy::shouldFireTimer(a, b) == false);
		QVERIFY(WebFilterSessionPolicy::shouldFireTimer(b, b));
	}

	void restoreInactiveSession()
	{
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock(QUuid::createUuid(), {}));
	}

	void emergencyUnlockUsesSameCleanup()
	{
		QCOMPARE(WebFilterSessionPolicy::reasonName(WebFilterSession::StopReason::EmergencyUnlock),
				 QStringLiteral("EmergencyUnlock"));
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 10 * 60 * 1000, 1'000'000);
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock(session.sessionId, session.sessionId));
	}

	void emergencyAuthDelayGrowsThenCaps()
	{
		QCOMPARE(WebFilterSessionPolicy::authFailureDelayMs(0), 0);
		QCOMPARE(WebFilterSessionPolicy::authFailureDelayMs(1), 500);
		QCOMPARE(WebFilterSessionPolicy::authFailureDelayMs(2), 1000);
		QCOMPARE(WebFilterSessionPolicy::authFailureDelayMs(20),
				 WebFilterSessionPolicy::AuthFailureMaxDelayMs);
	}

	void malformedPersistedStateFailsOpen()
	{
		bool ok = true;
		const auto session = WebFilterSession::fromJsonText(QStringLiteral("{not-json"), &ok);
		QVERIFY(ok == false);
		QVERIFY(session.isActive() == false);
		QString reason;
		QCOMPARE(WebFilterSessionPolicy::recoveryAction(session, 1, {}, &reason),
				 WebFilterSessionPolicy::RecoveryAction::FailOpen);
	}

	void unsupportedVersionFailsOpen()
	{
		auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 10 * 60 * 1000, 1'000'000);
		session.version = 99;
		QString reason;
		QCOMPARE(WebFilterSessionPolicy::recoveryAction(session, session.startTimeMs + 1000, {}, &reason),
				 WebFilterSessionPolicy::RecoveryAction::FailOpen);
		QVERIFY(reason.contains(QLatin1String("version")));
	}

	void missingSessionJsonReconstructsInsteadOfFailOpen()
	{
		WebFilterSession loaded;
		loaded.mode = WebFilterSession::Mode::Blacklist;
		loaded.domains = {QStringLiteral("8ballpool.com")};
		QString reason;
		QCOMPARE(WebFilterSessionPolicy::recoveryAction(loaded, 1'000'000, {}, &reason),
				 WebFilterSessionPolicy::RecoveryAction::FailOpen);
		const auto recovered = WebFilterSessionPolicy::recoverForReboot(
					loaded, 1'000'000, WebFilterSessionPolicy::DefaultMaxTtlMs, &reason);
		QVERIFY(recovered.isActive());
		QCOMPARE(recovered.mode, WebFilterSession::Mode::Blacklist);
		QVERIFY(recovered.sessionId.isNull() == false);
		QCOMPARE(WebFilterSessionPolicy::recoveryAction(recovered, 1'000'000, {}, &reason),
				 WebFilterSessionPolicy::RecoveryAction::Reapply);
	}

	void blacklistSettingsSurviveExpiry()
	{
		const auto blocked = WebFilterLists::toJson({QStringLiteral("8ballpool.com")});
		QVERIFY(blocked.empty() == false);
		auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 10 * 60 * 1000, 1'000'000,
					WebFilterSessionPolicy::DefaultMaxTtlMs, {QStringLiteral("8ballpool.com")});
		QCOMPARE(WebFilterLists::fromJson(blocked), QStringList{QStringLiteral("8ballpool.com")});
	}

	void whitelistSettingsSurviveExpiry()
	{
		const auto allowed = WebFilterLists::toJson({QStringLiteral("classroom.google.com")});
		auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 10 * 60 * 1000, 1'000'000,
					WebFilterSessionPolicy::DefaultMaxTtlMs, {QStringLiteral("classroom.google.com")});
		QCOMPARE(WebFilterLists::fromJson(allowed), QStringList{QStringLiteral("classroom.google.com")});
	}

	void extraProxyListSurvivesExpiry()
	{
		const auto extra = WebFilterLists::toJson({QStringLiteral("schoolproxy.test")});
		QCOMPARE(WebFilterLists::fromJson(extra), QStringList{QStringLiteral("schoolproxy.test")});
	}

	void builtInRulesRemainAfterClear()
	{
		QVERIFY(WebFilterLists::hardcodedProxyDomains().contains(QStringLiteral("croxyproxy.com")));
		QVERIFY(WebFilterLists::hardcodedDohDomains().contains(QStringLiteral("dns.google")));
		QVERIFY(WebFilterLists::hardcodedProxyDomains().contains(QStringLiteral("croxyproxy.com")));
		QVERIFY(WebFilterLists::isHardcodedBlocked(QStringLiteral("dns.google")));
	}

	void sessionReplacement()
	{
		const auto first = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 10 * 60 * 1000, 1'000'000);
		const auto second = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 20 * 60 * 1000, 1'100'000);
		QVERIFY(first.sessionId != second.sessionId);
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock(first.sessionId, second.sessionId) == false);
	}

	void startRestoreStartUsesNewSession()
	{
		const auto a = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 10 * 60 * 1000, 1'000'000);
		const auto b = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 15 * 60 * 1000, 1'200'000);
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock(a.sessionId, b.sessionId) == false);
		QVERIFY(a.sessionId != b.sessionId);
	}

	void reconnectResyncUsesCurrentSession()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 25 * 60 * 1000, 1'000'000);
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock(session.sessionId, session.sessionId));
		QVERIFY(WebFilterSessionPolicy::shouldApplyUnlock(QUuid::createUuid(), session.sessionId) == false);
	}

	void clampDurationAndCustomEnd()
	{
		QCOMPARE(WebFilterSessionPolicy::clampDurationMs(1000), WebFilterSessionPolicy::MinDurationMs);
		QCOMPARE(WebFilterSessionPolicy::clampDurationMs(10LL * 60 * 60 * 1000),
				 WebFilterSessionPolicy::DefaultMaxTtlMs);
		QCOMPARE(WebFilterSessionPolicy::durationUntilEndMs(1000, 1000 + 30 * 60 * 1000), 30 * 60 * 1000);
	}

	void remainingMsNeverNegative()
	{
		QCOMPARE(WebFilterSessionPolicy::remainingMs(10 * 60 * 1000, 20 * 60 * 1000), 0);
		QCOMPARE(WebFilterSessionPolicy::remainingMs(10 * 60 * 1000, 2 * 60 * 1000), 8 * 60 * 1000);
	}
};

QTEST_GUILESS_MAIN(WebFilterSessionPolicyTest)
#include "WebFilterSessionPolicyTest.moc"
