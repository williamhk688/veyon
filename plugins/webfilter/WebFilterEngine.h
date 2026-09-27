/*
 * WebFilterEngine.h - apply or restore classroom web filter rules
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QStringList>
#include <QUuid>

#include "WebFilterSession.h"

class WebFilterEngine
{
public:
	static qint64 configuredMaxTtlMs();
	static bool applyBlacklist(const QStringList& schoolBlocked, const QStringList& extraProxies = {},
							  bool restartBrowsers = true,
							  const WebFilterSession& session = {});
	static bool applyWhitelist(const QStringList& schoolAllowed, const QStringList& extraProxies = {},
							  bool restartBrowsers = true,
							  const WebFilterSession& session = {});
	static bool restore(bool restartBrowsers = true);
	static bool stopSession(const QUuid& sessionId,
							WebFilterSession::StopReason reason,
							bool restartBrowsers = true);
	static bool reconcileOnServiceStart();
	static void ensureClassroomFirewall();
};
