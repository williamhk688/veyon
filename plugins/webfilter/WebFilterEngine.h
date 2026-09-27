/*
 * WebFilterEngine.h - apply or restore classroom web filter rules
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QStringList>

class WebFilterEngine
{
public:
	static bool applyBlacklist(const QStringList& schoolBlocked, const QStringList& extraProxies = {},
							  bool restartBrowsers = true);
	static bool applyWhitelist(const QStringList& schoolAllowed, const QStringList& extraProxies = {},
							  bool restartBrowsers = true);
	static bool restore(bool restartBrowsers = true);
	static bool reconcileOnServiceStart();
};
