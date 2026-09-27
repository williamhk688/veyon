/*
 * ClassroomPersistStoreTest.cpp - ProgramData persist mirror
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 */

#include <QtTest>

#include <QDir>
#include <QFile>

#include "ClassroomPersistStore.h"

class ClassroomPersistStoreTest : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase()
	{
		m_dir = QDir::temp().absoluteFilePath(QStringLiteral("veyon-classroom-persist-test"));
		QDir(m_dir).removeRecursively();
		QVERIFY(QDir().mkpath(m_dir));
		qputenv("VEYON_CLASSROOM_PERSIST_DIR", m_dir.toUtf8());
	}

	void cleanup()
	{
		QDir(m_dir).removeRecursively();
		QVERIFY(QDir().mkpath(m_dir));
	}

	void cleanupTestCase()
	{
		QDir(m_dir).removeRecursively();
		qunsetenv("VEYON_CLASSROOM_PERSIST_DIR");
	}

	void writeReadRemoveRoundTrip()
	{
		QVERIFY(ClassroomPersistStore::writeValue(ClassroomPersistStore::screenLockName(),
												 QStringLiteral("ccb535a2-1d24-4cc1-a709-8b47d2b2ac79")));
		QCOMPARE(ClassroomPersistStore::readValue(ClassroomPersistStore::screenLockName()),
				 QStringLiteral("ccb535a2-1d24-4cc1-a709-8b47d2b2ac79"));
		QVERIFY(ClassroomPersistStore::removeValue(ClassroomPersistStore::screenLockName()));
		QVERIFY(ClassroomPersistStore::readValue(ClassroomPersistStore::screenLockName()).isEmpty());
	}

	void removeClassroomLocksKeepsWebFilter()
	{
		QVERIFY(ClassroomPersistStore::writeValue(ClassroomPersistStore::screenLockName(), QStringLiteral("lock")));
		QVERIFY(ClassroomPersistStore::writeValue(ClassroomPersistStore::demoName(), QStringLiteral("{\"lockInput\":true}")));
		QVERIFY(ClassroomPersistStore::writeValue(ClassroomPersistStore::webFilterModeName(), QStringLiteral("Blacklist")));
		QVERIFY(ClassroomPersistStore::writeValue(ClassroomPersistStore::webFilterSessionName(), QStringLiteral("{}")));
		QVERIFY(ClassroomPersistStore::removeClassroomLocks());
		QVERIFY(ClassroomPersistStore::readValue(ClassroomPersistStore::screenLockName()).isEmpty());
		QVERIFY(ClassroomPersistStore::readValue(ClassroomPersistStore::demoName()).isEmpty());
		QCOMPARE(ClassroomPersistStore::readValue(ClassroomPersistStore::webFilterModeName()),
				 QStringLiteral("Blacklist"));
		QCOMPARE(ClassroomPersistStore::readValue(ClassroomPersistStore::webFilterSessionName()),
				 QStringLiteral("{}"));
	}

	void removeWebFilterClearsSessionModeAndDomains()
	{
		QVERIFY(ClassroomPersistStore::writeValue(ClassroomPersistStore::webFilterModeName(), QStringLiteral("Whitelist")));
		QVERIFY(ClassroomPersistStore::writeValue(ClassroomPersistStore::webFilterSessionName(), QStringLiteral("{\"mode\":\"Whitelist\"}")));
		QVERIFY(ClassroomPersistStore::writeValue(ClassroomPersistStore::webFilterDomainsName(), QStringLiteral("classroom.google.com")));
		QVERIFY(ClassroomPersistStore::removeWebFilter());
		QVERIFY(ClassroomPersistStore::readValue(ClassroomPersistStore::webFilterModeName()).isEmpty());
		QVERIFY(ClassroomPersistStore::readValue(ClassroomPersistStore::webFilterSessionName()).isEmpty());
		QVERIFY(ClassroomPersistStore::readValue(ClassroomPersistStore::webFilterDomainsName()).isEmpty());
	}

private:
	QString m_dir;
};

QTEST_GUILESS_MAIN(ClassroomPersistStoreTest)
#include "ClassroomPersistStoreTest.moc"
