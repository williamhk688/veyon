/*
 * WebFilterEngine.cpp - non-Windows web filter is a no-op
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#if !defined(_WIN32)

#include "VeyonCore.h"
#include "WebFilterEngine.h"

bool WebFilterEngine::applyBlacklist(const QStringList&)
{
	vWarning() << "web filter is only implemented on Windows";
	return false;
}

bool WebFilterEngine::applyWhitelist(const QStringList&)
{
	vWarning() << "web filter is only implemented on Windows";
	return false;
}

bool WebFilterEngine::restore()
{
	return true;
}

bool WebFilterEngine::reconcileOnServiceStart()
{
	return true;
}

#endif
