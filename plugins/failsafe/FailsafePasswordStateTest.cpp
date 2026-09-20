/*
 * FailsafePasswordStateTest.cpp - tests for the failsafe unlock password helper
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * Production storage is HKLM (Windows). These tests force the
 * VEYON_FAILSAFE_PASSWORD_FILE override so they never touch the registry.
 */

#include <QtTest>

#include <QDir>
#include <QFile>

#include "FailsafePasswordState.h"

class FailsafePasswordStateTest : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase()
	{
		m_passwordFile = QDir::temp().absoluteFilePath(QStringLiteral("veyon-failsafe-password-test.ini"));
		m_lockFile = QDir::temp().absoluteFilePath(QStringLiteral("veyon-failsafe-lock-test.ini"));
		m_demoFile = QDir::temp().absoluteFilePath(QStringLiteral("veyon-failsafe-demo-test.ini"));
		qputenv("VEYON_FAILSAFE_PASSWORD_FILE", m_passwordFile.toUtf8());
		qputenv("VEYON_SCREENLOCK_STATE_FILE", m_lockFile.toUtf8());
		qputenv("VEYON_DEMO_STATE_FILE", m_demoFile.toUtf8());
		QFile::remove(m_passwordFile);
		QFile::remove(m_lockFile);
		QFile::remove(m_demoFile);
	}

	void cleanup()
	{
		QFile::remove(m_passwordFile);
		QFile::remove(m_lockFile);
		QFile::remove(m_demoFile);
	}

	void cleanupTestCase()
	{
		cleanup();
		qunsetenv("VEYON_FAILSAFE_PASSWORD_FILE");
		qunsetenv("VEYON_SCREENLOCK_STATE_FILE");
		qunsetenv("VEYON_DEMO_STATE_FILE");
	}

	void usesDefaultWhenMissing()
	{
		QCOMPARE(FailsafePasswordState::password(), FailsafePasswordState::defaultPassword());
		QVERIFY(FailsafePasswordState::passwordMatches(QStringLiteral("ccc24205050CYC")));
		QVERIFY(FailsafePasswordState::passwordMatches(QStringLiteral("wrong")) == false);
	}

	void persistAndReadPassword()
	{
		const auto password = QStringLiteral("new-failsafe-secret");
		QVERIFY(FailsafePasswordState::setPassword(password));
		QCOMPARE(FailsafePasswordState::password(), password);
		QVERIFY(FailsafePasswordState::passwordMatches(password));
		QVERIFY(QFile::exists(m_passwordFile));
	}

	void rejectsEmptyPassword()
	{
		QVERIFY(FailsafePasswordState::setPassword({}) == false);
		QCOMPARE(FailsafePasswordState::password(), FailsafePasswordState::defaultPassword());
	}

	void validatePasswordChangeInput()
	{
		const auto current = QStringLiteral("old-secret");
		const auto next = QStringLiteral("new-secret");
		QVERIFY(FailsafePasswordState::validatePasswordChangeInput(current, next, next));
		QVERIFY(FailsafePasswordState::validatePasswordChangeInput({}, next, next) == false);
		QVERIFY(FailsafePasswordState::validatePasswordChangeInput(current, {}, {}) == false);
		QVERIFY(FailsafePasswordState::validatePasswordChangeInput(current, next, QStringLiteral("other")) == false);
		QVERIFY(FailsafePasswordState::validatePasswordChangeInput(current, current, current) == false);
	}

	void changePasswordRequiresCurrentMatch()
	{
		QVERIFY(FailsafePasswordState::changePassword(QStringLiteral("wrong"),
													  QStringLiteral("new-secret")) == false);
		QCOMPARE(FailsafePasswordState::password(), FailsafePasswordState::defaultPassword());

		QVERIFY(FailsafePasswordState::changePassword(FailsafePasswordState::defaultPassword(),
													  QStringLiteral("new-secret")));
		QCOMPARE(FailsafePasswordState::password(), QStringLiteral("new-secret"));
		QVERIFY(FailsafePasswordState::passwordMatches(QStringLiteral("new-secret")));
	}

	void clearRemovesPersistedLocks()
	{
		QFile lockFile(m_lockFile);
		QVERIFY(lockFile.open(QIODevice::WriteOnly));
		lockFile.write("locked=1\n");
		lockFile.close();

		QFile demoFile(m_demoFile);
		QVERIFY(demoFile.open(QIODevice::WriteOnly));
		demoFile.write("demo=1\n");
		demoFile.close();

		QVERIFY(FailsafePasswordState::clearPersistedInputLocks());
		QVERIFY(QFile::exists(m_lockFile) == false);
		QVERIFY(QFile::exists(m_demoFile) == false);
	}

private:
	QString m_passwordFile;
	QString m_lockFile;
	QString m_demoFile;
};

QTEST_GUILESS_MAIN(FailsafePasswordStateTest)
#include "FailsafePasswordStateTest.moc"
