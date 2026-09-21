/*
 * WakeOnLanPacketTest.cpp - tests for WOL magic packet and IPv4 broadcast helpers
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QTest>

#include "WakeOnLanPacket.h"

class WakeOnLanPacketTest : public QObject
{
	Q_OBJECT
private Q_SLOTS:
	void magicPacketLayout()
	{
		const auto mac = QByteArray::fromHex("aabbccddeeff");
		const auto packet = WakeOnLanPacket::magicPacket(mac);
		QCOMPARE(packet.size(), 102);
		QCOMPARE(packet.left(6), QByteArray(6, char(0xff)));
		for (int i = 1; i < 17; ++i)
		{
			QCOMPARE(packet.mid(i * 6, 6), mac);
		}
	}

	void directedBroadcastFromPrefix()
	{
		const QHostAddress ip(QStringLiteral("10.81.0.119"));
		const auto broadcast = WakeOnLanPacket::directedBroadcast(ip, 23);
		QCOMPARE(broadcast, QHostAddress(QStringLiteral("10.81.1.255")));
	}

	void directedBroadcastFromMask()
	{
		const QHostAddress ip(QStringLiteral("10.81.0.119"));
		const QHostAddress mask(QStringLiteral("255.255.254.0"));
		const auto broadcast = WakeOnLanPacket::directedBroadcast(ip, mask);
		QCOMPARE(broadcast, QHostAddress(QStringLiteral("10.81.1.255")));
	}

	void subnetContainsHostOnStudentLan()
	{
		const QHostAddress teacher(QStringLiteral("10.81.0.119"));
		QVERIFY(WakeOnLanPacket::subnetContains(teacher, 23, QHostAddress(QStringLiteral("10.81.1.10"))));
		QVERIFY(!WakeOnLanPacket::subnetContains(teacher, 23, QHostAddress(QStringLiteral("192.168.21.158"))));
	}

	void rejectsInvalidMac()
	{
		QVERIFY(WakeOnLanPacket::magicPacket(QByteArray::fromHex("aa")).isEmpty());
		QCOMPARE(WakeOnLanPacket::normalizedMacBytes(QStringLiteral("AA:BB:CC:DD:EE:FF")).toHex(),
				 QByteArray("aabbccddeeff"));
	}
};

QTEST_GUILESS_MAIN(WakeOnLanPacketTest)
#include "WakeOnLanPacketTest.moc"
