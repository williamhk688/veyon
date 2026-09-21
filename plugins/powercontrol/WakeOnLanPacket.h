/*
 * WakeOnLanPacket.h - helpers for Wake-on-LAN magic packets and IPv4 broadcasts
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QString>

namespace WakeOnLanPacket
{

constexpr auto MacAddressSize = 6;
constexpr auto MagicPacketFieldCount = 17;
constexpr auto WolUdpPort = quint16(9);
constexpr auto RetryCount = 3;
constexpr auto RetryIntervalMs = 150;
constexpr auto PostSendDelayMs = 500;

inline QByteArray normalizedMacBytes(QString macAddress)
{
	macAddress.replace(QLatin1Char(':'), QString());
	macAddress.replace(QLatin1Char('-'), QString());
	macAddress.replace(QLatin1Char('.'), QString());
	return QByteArray::fromHex(macAddress.toUtf8());
}

inline QByteArray magicPacket(const QByteArray& macAddressBytes)
{
	if (macAddressBytes.size() != MacAddressSize)
	{
		return {};
	}

	QByteArray datagram(MacAddressSize * MagicPacketFieldCount, char(0xff));
	for (int i = 1; i < MagicPacketFieldCount; ++i)
	{
		datagram.replace(i * MacAddressSize, MacAddressSize, macAddressBytes);
	}
	return datagram;
}

inline quint32 prefixToMask(int prefixLength)
{
	if (prefixLength <= 0)
	{
		return 0;
	}
	if (prefixLength >= 32)
	{
		return 0xffffffffu;
	}
	return (~quint32(0)) << (32 - prefixLength);
}

inline QHostAddress directedBroadcast(const QHostAddress& ipv4, int prefixLength)
{
	if (ipv4.protocol() != QAbstractSocket::IPv4Protocol)
	{
		return {};
	}

	const quint32 mask = prefixToMask(prefixLength);
	return QHostAddress(ipv4.toIPv4Address() | ~mask);
}

inline QHostAddress directedBroadcast(const QHostAddress& ipv4, const QHostAddress& netmask)
{
	if (ipv4.protocol() != QAbstractSocket::IPv4Protocol ||
		netmask.protocol() != QAbstractSocket::IPv4Protocol)
	{
		return {};
	}

	return QHostAddress(ipv4.toIPv4Address() | ~netmask.toIPv4Address());
}

inline bool subnetContains(const QHostAddress& networkIp, int prefixLength, const QHostAddress& host)
{
	if (networkIp.protocol() != QAbstractSocket::IPv4Protocol ||
		host.protocol() != QAbstractSocket::IPv4Protocol)
	{
		return false;
	}

	const quint32 mask = prefixToMask(prefixLength);
	return (networkIp.toIPv4Address() & mask) == (host.toIPv4Address() & mask);
}

}
