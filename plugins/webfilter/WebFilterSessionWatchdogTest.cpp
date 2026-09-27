/*
 * WebFilterSessionWatchdogTest.cpp - local expiry poll without a Qt event loop
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 */

#include <QtTest>

#include <QDir>
#include <QFile>

#include "PersistentWebFilterState.h"
#include "WebFilterSessionPolicy.h"
#include "WebFilterSessionWatchdog.h"

class WebFilterSessionWatchdogTest : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase()
	{
		m_stateFile = QDir::temp().absoluteFilePath(QStringLiteral("veyon-webfilter-watchdog-test.ini"));
		qputenv("VEYON_WEBFILTER_STATE_FILE", m_stateFile.toUtf8());
		qputenv("VEYON_FAILSAFE_PASSWORD_FILE",
				QDir::temp().absoluteFilePath(QStringLiteral("veyon-webfilter-watchdog-failsafe.ini")).toUtf8());
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
		qunsetenv("VEYON_FAILSAFE_PASSWORD_FILE");
	}

	void inactiveSessionDoesNotExpire()
	{
		WebFilterSessionWatchdog watchdog;
		QVERIFY(watchdog.poll() == false);
	}

	void disconnectKeepsSessionUntilLocalDuration()
	{
		const auto session = WebFilterSessionPolicy::create(
					WebFilterSession::Mode::Whitelist, 60 * 1000, 1'000'000);
		QCOMPARE(WebFilterSessionPolicy::liveAction(session, 5 * 1000),
				 WebFilterSessionPolicy::LiveAction::Continue);
		QCOMPARE(WebFilterSessionPolicy::liveAction(session, session.durationMs),
				 WebFilterSessionPolicy::LiveAction::ExpireDuration);
	}

private:
	QString m_stateFile;
};

QTEST_GUILESS_MAIN(WebFilterSessionWatchdogTest)
#include "WebFilterSessionWatchdogTest.moc"
