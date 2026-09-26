/*
 * WebFilterLists.h - normalize domains and build effective allow/deny lists
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QJsonArray>
#include <QStringList>

class WebFilterLists
{
public:
	static QString normalizeDomain(const QString& raw);
	static QStringList normalizeDomains(const QStringList& raw);
	static QStringList hardcodedProxyDomains();
	static QStringList hardcodedDohDomains();
	static QStringList defaultBlockedDomains();
	static QStringList defaultAllowedDomains();
	static QJsonArray toJson(const QStringList& domains);
	static QStringList fromJson(const QJsonArray& domains);

	static QStringList effectiveBlacklist(const QStringList& schoolBlocked,
										 const QStringList& extraProxies = {});
	static QStringList effectiveAllowlist(const QStringList& schoolAllowed,
										 const QStringList& extraProxies = {});
	static bool isHardcodedBlocked(const QString& domain);
	static bool isAlwaysBlocked(const QString& domain, const QStringList& extraProxies = {});
	static QStringList chromePolicyPatterns(const QStringList& domains);
	static QStringList chromeUrlPatterns(const QStringList& domains);
	static QStringList hostsNames(const QStringList& domains);
	static QStringList browserAllowPatterns(const QStringList& domains);
	static QStringList firefoxMatchPatterns(const QStringList& domains);
	static QString proxyPacScript(bool whitelistMode,
								 const QStringList& domains,
								 const QStringList& extraBlocked = {});

	static constexpr auto HostsBeginMarker = "# ----- VEYON-WEBFILTER-BEGIN -----";
	static constexpr auto HostsEndMarker = "# ----- VEYON-WEBFILTER-END -----";

	static QByteArray replaceHostsSection(const QByteArray& existing, const QStringList& domains);
	static QByteArray removeHostsSection(const QByteArray& existing);
	static bool hostsSectionPresent(const QByteArray& existing);
};
