/*
 * WebFilterListsTest.cpp - tests for domain normalization and list merge
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 */

#include <QtTest>

#include "WebFilterLists.h"

class WebFilterListsTest : public QObject
{
	Q_OBJECT
private slots:
	void normalizeStripsSchemeAndPath()
	{
		QCOMPARE(WebFilterLists::normalizeDomain(QStringLiteral("https://WWW.CroxyProxy.com/foo")),
				 QStringLiteral("www.croxyproxy.com"));
		QCOMPARE(WebFilterLists::normalizeDomain(QStringLiteral("*.example.com")),
				 QStringLiteral("example.com"));
		QCOMPARE(WebFilterLists::normalizeDomain(QStringLiteral("  ")), QString());
	}

	void hardcodedProxyCannotBeAllowed()
	{
		QVERIFY(WebFilterLists::isHardcodedBlocked(QStringLiteral("www.croxyproxy.com")));
		QVERIFY(WebFilterLists::isHardcodedBlocked(QStringLiteral("dns.google")));
		QVERIFY(WebFilterLists::isHardcodedBlocked(QStringLiteral("translate.google.com")) == false);
		const auto allowed = WebFilterLists::effectiveAllowlist({
			QStringLiteral("classroom.google.com"),
			QStringLiteral("croxyproxy.com"),
			QStringLiteral("translate.google.com")
		});
		QVERIFY(allowed.contains(QStringLiteral("classroom.google.com")));
		QVERIFY(allowed.contains(QStringLiteral("translate.google.com")));
		QVERIFY(allowed.contains(QStringLiteral("croxyproxy.com")) == false);
	}

	void blacklistAlwaysIncludesProxies()
	{
		const auto list = WebFilterLists::effectiveBlacklist({QStringLiteral("pornhub.com")},
															 {QStringLiteral("schoolproxy.test")});
		QVERIFY(list.contains(QStringLiteral("pornhub.com")));
		QVERIFY(list.contains(QStringLiteral("proxysite.com")));
		QVERIFY(list.contains(QStringLiteral("kproxy.com")));
		QVERIFY(list.contains(QStringLiteral("hide.me")));
		QVERIFY(list.contains(QStringLiteral("dns.google")));
		QVERIFY(list.contains(QStringLiteral("schoolproxy.test")));
	}

	void extraProxyCannotBeAllowed()
	{
		const auto allowed = WebFilterLists::effectiveAllowlist(
			{QStringLiteral("classroom.google.com"), QStringLiteral("schoolproxy.test")},
			{QStringLiteral("schoolproxy.test")});
		QVERIFY(allowed.contains(QStringLiteral("classroom.google.com")));
		QVERIFY(allowed.contains(QStringLiteral("schoolproxy.test")) == false);
	}

	void chromePolicyUsesHostDotSyntax()
	{
		const auto patterns = WebFilterLists::chromePolicyPatterns({QStringLiteral("classroom.google.com")});
		QVERIFY(patterns.contains(QStringLiteral("classroom.google.com")));
		QVERIFY(patterns.contains(QStringLiteral(".classroom.google.com")));
		for (const auto& pattern : patterns)
		{
			QVERIFY(pattern.contains(QLatin1String("*://")) == false);
		}
	}

	void whitelistPacAllowsListedHost()
	{
		const auto pac = WebFilterLists::proxyPacScript(
			true, {QStringLiteral("classroom.google.com")}, {QStringLiteral("schoolproxy.test")});
		QVERIFY(pac.contains(QLatin1String("function FindProxyForURL")));
		QVERIFY(pac.contains(QLatin1String("hostMatches(host, \"classroom.google.com\")")) );
		QVERIFY(pac.contains(QLatin1String("return \"DIRECT\"")));
		QVERIFY(pac.contains(QLatin1String("schoolproxy.test")));
		QVERIFY(pac.contains(QLatin1String("PROXY 127.0.0.1:9")));
	}

	void hostsSectionRoundTrip()
	{
		const auto original = QByteArrayLiteral("127.0.0.1 localhost\n");
		const auto withSection = WebFilterLists::replaceHostsSection(
			original, {QStringLiteral("croxyproxy.com")});
		QVERIFY(withSection.contains(QByteArray(WebFilterLists::HostsBeginMarker)));
		QVERIFY(withSection.contains(QByteArrayLiteral("0.0.0.0 croxyproxy.com")));
		QVERIFY(withSection.contains(QByteArrayLiteral("0.0.0.0 www.croxyproxy.com")));
		const auto restored = WebFilterLists::removeHostsSection(withSection);
		QCOMPARE(QString::fromUtf8(restored).trimmed(), QStringLiteral("127.0.0.1 localhost"));
		QVERIFY(WebFilterLists::hostsSectionPresent(withSection));
		QVERIFY(WebFilterLists::hostsSectionPresent(original) == false);
	}
};

QTEST_GUILESS_MAIN(WebFilterListsTest)
#include "WebFilterListsTest.moc"
