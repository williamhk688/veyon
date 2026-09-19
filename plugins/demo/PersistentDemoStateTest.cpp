/*
 * PersistentDemoStateTest.cpp - tests for persisted demo state
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * Production persistence is HKLM (Windows). These tests force the
 * VEYON_DEMO_STATE_FILE override so they never touch the registry.
 */

#include <QtTest>

#include <QDir>
#include <QFile>

#include "PersistentDemoState.h"

class PersistentDemoStateTest : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase()
	{
		m_stateFile = QDir::temp().absoluteFilePath(QStringLiteral("veyon-demo-state-test.ini"));
		qputenv("VEYON_DEMO_STATE_FILE", m_stateFile.toUtf8());
		QFile::remove(m_stateFile);
	}

	void cleanup()
	{
		QFile::remove(m_stateFile);
	}

	void cleanupTestCase()
	{
		QFile::remove(m_stateFile);
		qunsetenv("VEYON_DEMO_STATE_FILE");
	}

	void inactiveByDefault()
	{
		QVERIFY(PersistentDemoState::isActive() == false);
		QVERIFY(PersistentDemoState::lockInput() == false);
		QVERIFY(PersistentDemoState::snapshot().featureUid.isNull());
	}

	void persistAndReadFullscreenDemo()
	{
		PersistentDemoState::Snapshot snapshot;
		snapshot.featureUid = Feature::Uid{QStringLiteral("7b6231bd-eb89-45d3-af32-f70663b2f878")};
		snapshot.demoServerHost = QStringLiteral("10.0.0.5");
		snapshot.demoServerPort = 11400;
		snapshot.demoAccessToken = QByteArray::fromHex("aabbccdd");
		snapshot.viewport = QRect(0, 0, 1920, 1080);
		snapshot.lockInput = true;

		QVERIFY(PersistentDemoState::setActive(snapshot));
		QVERIFY(PersistentDemoState::isActive());
		QVERIFY(PersistentDemoState::lockInput());

		const auto restored = PersistentDemoState::snapshot();
		QCOMPARE(restored.featureUid, snapshot.featureUid);
		QCOMPARE(restored.demoServerHost, snapshot.demoServerHost);
		QCOMPARE(restored.demoServerPort, snapshot.demoServerPort);
		QCOMPARE(restored.demoAccessToken, snapshot.demoAccessToken);
		QCOMPARE(restored.viewport, snapshot.viewport);
		QVERIFY(restored.lockInput);
		QVERIFY(QFile::exists(m_stateFile));
	}

	void clearRemovesDemo()
	{
		PersistentDemoState::Snapshot snapshot;
		snapshot.featureUid = Feature::Uid{QStringLiteral("ae45c3db-dc2e-4204-ae8b-374cdab8c62c")};
		snapshot.demoServerHost = QStringLiteral("teacher.local");
		snapshot.demoServerPort = 11400;
		QVERIFY(PersistentDemoState::setActive(snapshot));
		QVERIFY(PersistentDemoState::clear());
		QVERIFY(PersistentDemoState::isActive() == false);
		QVERIFY(QFile::exists(m_stateFile) == false);
	}

private:
	QString m_stateFile;
};

QTEST_GUILESS_MAIN(PersistentDemoStateTest)
#include "PersistentDemoStateTest.moc"
