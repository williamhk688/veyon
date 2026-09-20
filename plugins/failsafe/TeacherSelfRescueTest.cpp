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
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("1969"),
												QStringLiteral("DAT"),
												QStringLiteral("WAN")));
	}

	void ignoresCaseAndSpaces()
	{
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral(" 1969 "),
												QStringLiteral("dat"),
												QStringLiteral("wan")));
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("1969"),
												QStringLiteral("Dat"),
												QStringLiteral("Wan")));
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("1969"),
												QStringLiteral("D A T"),
												QStringLiteral("W A N")));
	}

	void acceptsCommonSuffixes()
	{
		QVERIFY(TeacherSelfRescue::answersMatch(QString::fromUtf8("1969年"),
												QString::fromUtf8("DAT室"),
												QStringLiteral("WAN Sir")));
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("1969"),
												QStringLiteral("DAT room"),
												QStringLiteral("wan sir")));
	}

	void rejectsWrongAnswers()
	{
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("1970"),
												QStringLiteral("DAT"),
												QStringLiteral("WAN")) == false);
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("1969"),
												QStringLiteral("STEM"),
												QStringLiteral("WAN")) == false);
		QVERIFY(TeacherSelfRescue::answersMatch(QStringLiteral("1969"),
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
		QCOMPARE(TeacherSelfRescue::normalizeAnswer(QString::fromUtf8("1969年")),
				 QStringLiteral("1969"));
	}
};

QTEST_GUILESS_MAIN(TeacherSelfRescueTest)
#include "TeacherSelfRescueTest.moc"
