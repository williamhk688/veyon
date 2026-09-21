/*
 * WindowsWolAdapterControl.h - enumerate, select, enable, and restore a
 * Windows Ethernet adapter for Wake-on-LAN
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include "PlatformNetworkFunctions.h"

class WindowsWolAdapterControl
{
public:
	static constexpr int LinkReadyTimeoutMs = 5000;
	static constexpr int LinkPollIntervalMs = 50;

	static std::unique_ptr<PlatformNetworkFunctions::WakeOnLanSession>
	acquireSession(const QList<QHostAddress>& targetHosts);

	static bool setAdminStatus(unsigned long interfaceIndex, bool enabled);
	static bool setAdminStatusNative(unsigned long interfaceIndex, bool enabled);
	static bool canTemporarilyEnableAdapter(unsigned long interfaceIndex);
};
