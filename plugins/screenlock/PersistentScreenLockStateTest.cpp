/*
 * PersistentScreenLockStateTest.cpp - tests for persisted screen-lock state
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QtTest>

#include <QDir>
#include <QFile>

#include "PersistentScreenLockState.h"

class PersistentScreenLockStateTest : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase()
	{
		m_stateFile = QDir::temp().absoluteFilePath(QStringLiteral("veyon-screenlock-state-test.ini"));
		qputenv("VEYON_SCREENLOCK_STATE_FILE", m_stateFile.toUtf8());
		QFile::remove(m_stateFile);
	}

	void cleanup()
	{
		QFile::remove(m_stateFile);
	}

	void cleanupTestCase()
	{
		QFile::remove(m_stateFile);
		qunsetenv("VEYON_SCREENLOCK_STATE_FILE");
	}

	void unlockedByDefault()
	{
		QVERIFY(PersistentScreenLockState::isLocked() == false);
		QVERIFY(PersistentScreenLockState::lockedFeatureUid().isNull());
	}

	void persistAndReadLock()
	{
		const Feature::Uid uid{QStringLiteral("ccb535a2-1d24-4cc1-a709-8b47d2b2ac79")};
		QVERIFY(PersistentScreenLockState::setLocked(uid));
		QVERIFY(PersistentScreenLockState::isLocked());
		QCOMPARE(PersistentScreenLockState::lockedFeatureUid(), uid);
		QVERIFY(QFile::exists(m_stateFile));
	}

	void clearRemovesLock()
	{
		const Feature::Uid uid{QStringLiteral("e4a77879-e544-4fec-bc18-e534f33b934c")};
		QVERIFY(PersistentScreenLockState::setLocked(uid));
		QVERIFY(PersistentScreenLockState::clear());
		QVERIFY(PersistentScreenLockState::isLocked() == false);
		QVERIFY(QFile::exists(m_stateFile) == false);
	}

	void survivesReread()
	{
		const Feature::Uid uid{QStringLiteral("ccb535a2-1d24-4cc1-a709-8b47d2b2ac79")};
		QVERIFY(PersistentScreenLockState::setLocked(uid));
		QCOMPARE(PersistentScreenLockState::lockedFeatureUid(), uid);
		QCOMPARE(PersistentScreenLockState::lockedFeatureUid(), uid);
	}

private:
	QString m_stateFile;
};

QTEST_GUILESS_MAIN(PersistentScreenLockStateTest)
#include "PersistentScreenLockStateTest.moc"
