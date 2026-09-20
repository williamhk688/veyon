/*
 * TeacherSelfRescueTest.cpp - tests for teacher self-rescue quiz matching
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QtTest>

#include "TeacherSelfRescue.h"

class TeacherSelfRescueTest : public QObject
{
	Q_OBJECT
private slots:
	void acceptsCanonicalAnswers()
	{
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("LCC"),
												QStringLiteral("DAT"),
												QStringLiteral("WAN")));
	}

	void ignoresCaseAndSpaces()
	{
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral(" lcc "),
												QStringLiteral("dat"),
												QStringLiteral("wan")));
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("Lcc"),
												QStringLiteral("Dat"),
												QStringLiteral("Wan")));
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("L C C"),
												QStringLiteral("D A T"),
												QStringLiteral("W A N")));
	}

	void acceptsCommonSuffixes()
	{
		QVERIFY(TeacherSelfRescue::answersMatch(QString::fromUtf8("LCC老師"),
												QString::fromUtf8("DAT室"),
												QStringLiteral("WAN Sir")));
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("L.C.C."),
												QStringLiteral("DAT room"),
												QStringLiteral("wan sir")));
	}

	void rejectsWrongAnswers()
	{
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("1969"),
												QStringLiteral("DAT"),
												QStringLiteral("WAN")) == false);
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("LCC"),
												QStringLiteral("STEM"),
												QStringLiteral("WAN")) == false);
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("LCC"),
												QStringLiteral("DAT"),
												QStringLiteral("WONG")) == false);
		QVERIFY(TeacherSelfRescue::answersMatch({}, {}, {}) == false);
	}

	void normalizeStripsNoise()
	{
		QCOMPARE(TeacherSelfRescue::normalizeAnswer(QStringLiteral(" wan sir ")),
				 QStringLiteral("WAN"));
		QCOMPARE(TeacherSelfRescue::normalizeAnswer(QString::fromUtf8(" DAT 室 ")),
				 QStringLiteral("DAT"));
		QCOMPARE(TeacherSelfRescue::normalizeAnswer(QStringLiteral("l.c.c.")),
				 QStringLiteral("LCC"));
		QCOMPARE(TeacherSelfRescue::normalizeAnswer(QString::fromUtf8("LCC老師")),
				 QStringLiteral("LCC"));
	}
};

QTEST_GUILESS_MAIN(TeacherSelfRescueTest)
#include "TeacherSelfRescueTest.moc"
