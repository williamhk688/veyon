/*
 * PersistentWebFilterStateTest.cpp - file-backed state tests
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 */

#include <QtTest>

#include <QDir>
#include <QFile>

#include "PersistentWebFilterState.h"

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

private:
	QString m_stateFile;
};

QTEST_GUILESS_MAIN(PersistentWebFilterStateTest)
#include "PersistentWebFilterStateTest.moc"
