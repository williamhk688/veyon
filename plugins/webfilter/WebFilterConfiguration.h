/*
 * WebFilterConfiguration.h - Configurator lists for classroom web filtering
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include "VeyonConfiguration.h"
#include "Configuration/Proxy.h"

#define FOREACH_WEB_FILTER_CONFIG_PROPERTY(OP) \
	OP(WebFilterConfiguration, m_configuration, QJsonArray, blockedWebsites, setBlockedWebsites, "BlockedWebsites", "WebFilter", QJsonArray(), Configuration::Property::Flag::Standard) \
	OP(WebFilterConfiguration, m_configuration, QJsonArray, allowedWebsites, setAllowedWebsites, "AllowedWebsites", "WebFilter", QJsonArray(), Configuration::Property::Flag::Standard) \
	OP(WebFilterConfiguration, m_configuration, QJsonArray, extraProxyWebsites, setExtraProxyWebsites, "ExtraProxyWebsites", "WebFilter", QJsonArray(), Configuration::Property::Flag::Standard)

DECLARE_CONFIG_PROXY(WebFilterConfiguration, FOREACH_WEB_FILTER_CONFIG_PROPERTY)
