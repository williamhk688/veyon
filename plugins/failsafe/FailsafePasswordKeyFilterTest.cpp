/*
 * FailsafePasswordKeyFilterTest.cpp - tests for the failsafe password key whitelist
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QtTest>

#include "FailsafePasswordKeyFilter.h"

class FailsafePasswordKeyFilterTest : public QObject
{
	Q_OBJECT
private slots:
	void allowsLettersDigitsSymbolsEnterBackspace()
	{
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x1E, false)); // A
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x16, false)); // U
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x02, false)); // 1
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x0B, false)); // 0
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x0C, false)); // -
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x0E, false)); // Backspace
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x1C, false)); // Enter
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x1C, true));  // Numpad Enter
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x39, false)); // Space
	}

	void shiftIsModifierNotATypingKey()
	{
		QVERIFY(FailsafePasswordKeyFilter::isShiftKey(0x2A, false));
		QVERIFY(FailsafePasswordKeyFilter::isShiftKey(0x36, false));
		QVERIFY(FailsafePasswordKeyFilter::isShiftKey(0x2A, true) == false);
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x2A, false) == false);
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x36, false) == false);
		QVERIFY(FailsafePasswordKeyFilter::isBlockedSystemModifier(0x2A, false) == false);
	}

	void blocksEscapeChords()
	{
		QVERIFY(FailsafePasswordKeyFilter::isBlockedSystemModifier(0x1D, false)); // Ctrl
		QVERIFY(FailsafePasswordKeyFilter::isBlockedSystemModifier(0x1D, true));  // Right Ctrl
		QVERIFY(FailsafePasswordKeyFilter::isBlockedSystemModifier(0x38, false)); // Alt
		QVERIFY(FailsafePasswordKeyFilter::isBlockedSystemModifier(0x38, true));  // AltGr
		QVERIFY(FailsafePasswordKeyFilter::isBlockedSystemModifier(0x5B, true));  // Left Win
		QVERIFY(FailsafePasswordKeyFilter::isBlockedSystemModifier(0x5C, true));  // Right Win

		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x1D, false) == false);
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x38, false) == false);
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x5B, true) == false);
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x01, false) == false); // Esc
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x0F, false) == false); // Tab
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x3E, false) == false); // F4
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x53, true) == false);  // Delete
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x5D, true) == false);  // Menu
		QVERIFY(FailsafePasswordKeyFilter::isAllowedTypingKey(0x1D, false, true) == false); // Pause prefix
	}
};

QTEST_GUILESS_MAIN(FailsafePasswordKeyFilterTest)
#include "FailsafePasswordKeyFilterTest.moc"
