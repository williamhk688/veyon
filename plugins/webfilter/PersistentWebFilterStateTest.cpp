/*
 * PersistentWebFilterStateTest.cpp - file-backed state tests
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 */

#include <QtTest>

#include <QDir>
#include <QFile>

#include "PersistentWebFilterState.h"
#include "WebFilterSessionPolicy.h"

class PersistentWebFilterStateTest : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase()
	{
		m_stateFile = QDir::temp().absoluteFilePath(QStringLiteral("veyon-webfilter-state-test.ini"));
		qputenv("VEYON_WEBFILTER_STATE_FILE", m_stateFile.toUtf8());
		QFile::remove(m_stateFile);
	}

	void cleanup()
	{
		QFile::remove(m_stateFile);
	}

	void cleanupTestCase()
	{
		QFile::remove(m_stateFile);
		qunsetenv("VEYON_WEBFILTER_STATE_FILE");
	}

	void offByDefault()
	{
		QCOMPARE(PersistentWebFilterState::mode(), PersistentWebFilterState::Mode::Off);
		QVERIFY(PersistentWebFilterState::domains().isEmpty());
	}

	void persistBlacklist()
	{
		QVERIFY(PersistentWebFilterState::setBlacklist({QStringLiteral("pornhub.com"), QStringLiteral("CROXYPROXY.COM")}));
		QCOMPARE(PersistentWebFilterState::mode(), PersistentWebFilterState::Mode::Blacklist);
		QVERIFY(PersistentWebFilterState::domains().contains(QStringLiteral("pornhub.com")));
		QVERIFY(PersistentWebFilterState::domains().contains(QStringLiteral("croxyproxy.com")));
	}

	void persistWhitelistThenClear()
	{
		QVERIFY(PersistentWebFilterState::setWhitelist({QStringLiteral("classroom.google.com")}));
		QCOMPARE(PersistentWebFilterState::mode(), PersistentWebFilterState::Mode::Whitelist);
		QVERIFY(PersistentWebFilterState::clear());
		QCOMPARE(PersistentWebFilterState::mode(), PersistentWebFilterState::Mode::Off);
		QVERIFY(QFile::exists(m_stateFile) == false);
	}

	void snapshotRoundTrip()
	{
		QVERIFY(PersistentWebFilterState::setPolicySnapshot(QStringLiteral("{\"chromeBlock\":{}}")));
		QCOMPARE(PersistentWebFilterState::policySnapshot(), QStringLiteral("{\"chromeBlock\":{}}"));
	}

	void sessionRoundTripAndReplacement()
	{
		const auto first = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 30 * 60 * 1000, 1'000'000,
					WebFilterSessionPolicy::DefaultMaxTtlMs, {QStringLiteral("8ballpool.com")});
		QVERIFY(PersistentWebFilterState::saveSession(first));
		QCOMPARE(PersistentWebFilterState::session().sessionId, first.sessionId);
		QCOMPARE(PersistentWebFilterState::mode(), WebFilterSession::Mode::Blacklist);

		const auto second = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 20 * 60 * 1000, 1'100'000,
					WebFilterSessionPolicy::DefaultMaxTtlMs, {QStringLiteral("classroom.google.com")});
		QVERIFY(PersistentWebFilterState::saveSession(second));
		QCOMPARE(PersistentWebFilterState::session().sessionId, second.sessionId);
		QCOMPARE(PersistentWebFilterState::mode(), WebFilterSession::Mode::Whitelist);
		QVERIFY(PersistentWebFilterState::clear());
		QVERIFY(PersistentWebFilterState::session().isActive() == false);
	}

	void checkpointDoesNotClearMode()
	{
		auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 30 * 60 * 1000, 1'000'000);
		QVERIFY(PersistentWebFilterState::saveSession(session));
		QVERIFY(PersistentWebFilterState::updateCheckpoint(15 * 1000, 1'015'000));
		QCOMPARE(PersistentWebFilterState::session().checkpointElapsedMs, 15 * 1000);
		QCOMPARE(PersistentWebFilterState::session().bootId, session.bootId);
		QCOMPARE(PersistentWebFilterState::mode(), WebFilterSession::Mode::Blacklist);
	}

	void emergencyUnlockBlocksSameSessionOnly()
	{
		const auto first = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Blacklist, 30 * 60 * 1000, 1'000'000);
		const auto second = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 20 * 60 * 1000, 1'100'000);
		QVERIFY(PersistentWebFilterState::noteEmergencyUnlocked(first.sessionId));
		QVERIFY(PersistentWebFilterState::isEmergencyUnlocked(first.sessionId));
		QVERIFY(PersistentWebFilterState::shouldIgnoreApply(first.sessionId));
		QVERIFY(PersistentWebFilterState::shouldIgnoreApply(second.sessionId) == false);
		QVERIFY(PersistentWebFilterState::shouldIgnoreApply({}) == false);
		QVERIFY(PersistentWebFilterState::clearEmergencyUnlocked());
		QVERIFY(PersistentWebFilterState::shouldIgnoreApply(first.sessionId) == false);
		QVERIFY(PersistentWebFilterState::noteEmergencyUnlocked(first.sessionId));
		QVERIFY(PersistentWebFilterState::clear());
		QVERIFY(PersistentWebFilterState::shouldIgnoreApply(first.sessionId) == false);
		QVERIFY(PersistentWebFilterState::emergencyUnlockedSession().isNull());
	}

private:
	QString m_stateFile;
};

QTEST_GUILESS_MAIN(PersistentWebFilterStateTest)
#include "PersistentWebFilterStateTest.moc"
