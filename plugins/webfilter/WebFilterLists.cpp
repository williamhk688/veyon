/*
 * WebFilterLists.cpp - domain list helpers for classroom web filtering
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QSet>
#include <QUrl>

#include "WebFilterLists.h"


static QStringList uniqueSorted(const QStringList& domains)
{
	QSet<QString> unique;
	for (const auto& domain : domains)
	{
		if (domain.isEmpty() == false)
		{
			unique.insert(domain);
		}
	}

	auto list = QStringList(unique.begin(), unique.end());
	list.sort(Qt::CaseInsensitive);
	return list;
}


static bool isSubdomainOf(const QString& domain, const QString& parent)
{
	if (domain == parent)
	{
		return true;
	}

	return domain.endsWith(QLatin1Char('.') + parent);
}


QString WebFilterLists::normalizeDomain(const QString& raw)
{
	auto text = raw.trimmed().toLower();
	if (text.isEmpty() || text.startsWith(QLatin1Char('#')))
	{
		return {};
	}

	text.replace(QLatin1Char('\\'), QLatin1Char('/'));
	if (text.contains(QLatin1String("://")) == false && text.contains(QLatin1Char('/')))
	{
		text = QStringLiteral("http://") + text;
	}

	if (text.contains(QLatin1String("://")))
	{
		const QUrl url(text);
		text = url.host();
	}

	if (text.startsWith(QLatin1Char('[')) && text.endsWith(QLatin1Char(']')))
	{
		text = text.mid(1, text.size() - 2);
	}

	while (text.startsWith(QLatin1String("*.")))
	{
		text.remove(0, 2);
	}

	while (text.endsWith(QLatin1Char('.')))
	{
		text.chop(1);
	}

	const auto slash = text.indexOf(QLatin1Char('/'));
	if (slash >= 0)
	{
		text = text.left(slash);
	}

	if (text.count(QLatin1Char(':')) == 1)
	{
		text = text.left(text.indexOf(QLatin1Char(':')));
	}

	text = text.trimmed();
	if (text.isEmpty() || text.contains(QLatin1Char(' ')) || text.contains(QLatin1Char('\t')))
	{
		return {};
	}

	return text;
}


QStringList WebFilterLists::normalizeDomains(const QStringList& raw)
{
	QStringList normalized;
	normalized.reserve(raw.size());
	for (const auto& item : raw)
	{
		normalized.append(normalizeDomain(item));
	}
	return uniqueSorted(normalized);
}


QStringList WebFilterLists::hardcodedProxyDomains()
{
	return {
		QStringLiteral("blockaway.net"),
		QStringLiteral("croxy.network"),
		QStringLiteral("croxyproxy.com"),
		QStringLiteral("croxyproxy.rocks"),
		QStringLiteral("filterbypass.me"),
		QStringLiteral("hide.me"),
		QStringLiteral("hidemyass.com"),
		QStringLiteral("kproxy.com"),
		QStringLiteral("proxfree.com"),
		QStringLiteral("proxy-site.com"),
		QStringLiteral("proxyium.com"),
		QStringLiteral("proxysite.com"),
		QStringLiteral("whoer.net"),
		QStringLiteral("youtubeunblocked.live"),
		QStringLiteral("zend2.com"),
		QStringLiteral("4everproxy.com"),
	};
}


QStringList WebFilterLists::hardcodedDohDomains()
{
	return {
		QStringLiteral("chrome.cloudflare-dns.com"),
		QStringLiteral("cloudflare-dns.com"),
		QStringLiteral("dns.google"),
		QStringLiteral("dns.quad9.net"),
		QStringLiteral("doh.dns.sb"),
		QStringLiteral("doh.opendns.com"),
		QStringLiteral("mozilla.cloudflare-dns.com"),
	};
}


QStringList WebFilterLists::defaultBlockedDomains()
{
	return {
		QStringLiteral("1xbet.com"),
		QStringLiteral("188bet.com"),
		QStringLiteral("bet365.com"),
		QStringLiteral("pornhub.com"),
		QStringLiteral("spankbang.com"),
		QStringLiteral("xhamster.com"),
		QStringLiteral("xnxx.com"),
		QStringLiteral("xvideos.com"),
	};
}


QStringList WebFilterLists::defaultAllowedDomains()
{
	return {
		QStringLiteral("accounts.google.com"),
		QStringLiteral("calendar.google.com"),
		QStringLiteral("classroom.google.com"),
		QStringLiteral("docs.google.com"),
		QStringLiteral("drive.google.com"),
		QStringLiteral("google.com"),
		QStringLiteral("google.com.hk"),
		QStringLiteral("googleapis.com"),
		QStringLiteral("googleusercontent.com"),
		QStringLiteral("gstatic.com"),
		QStringLiteral("live.com"),
		QStringLiteral("login.microsoftonline.com"),
		QStringLiteral("mail.google.com"),
		QStringLiteral("microsoft.com"),
		QStringLiteral("microsoft365.com"),
		QStringLiteral("microsoftonline.com"),
		QStringLiteral("office.com"),
		QStringLiteral("office.net"),
		QStringLiteral("outlook.office.com"),
		QStringLiteral("sharepoint.com"),
	};
}


QJsonArray WebFilterLists::toJson(const QStringList& domains)
{
	QJsonArray array;
	for (const auto& domain : normalizeDomains(domains))
	{
		array.append(domain);
	}
	return array;
}


QStringList WebFilterLists::fromJson(const QJsonArray& domains)
{
	QStringList raw;
	raw.reserve(domains.size());
	for (const auto& value : domains)
	{
		if (value.isString())
		{
			raw.append(value.toString());
		}
	}
	return normalizeDomains(raw);
}


bool WebFilterLists::isHardcodedBlocked(const QString& domain)
{
	return isAlwaysBlocked(domain, {});
}


bool WebFilterLists::isAlwaysBlocked(const QString& domain, const QStringList& extraProxies)
{
	const auto normalized = normalizeDomain(domain);
	if (normalized.isEmpty())
	{
		return false;
	}

	const auto blocked = hardcodedProxyDomains() + hardcodedDohDomains() + normalizeDomains(extraProxies);
	for (const auto& parent : blocked)
	{
		if (isSubdomainOf(normalized, parent))
		{
			return true;
		}
	}

	return false;
}


QStringList WebFilterLists::effectiveBlacklist(const QStringList& schoolBlocked, const QStringList& extraProxies)
{
	return uniqueSorted(hardcodedProxyDomains() + hardcodedDohDomains() +
						normalizeDomains(extraProxies) + normalizeDomains(schoolBlocked));
}


QStringList WebFilterLists::effectiveAllowlist(const QStringList& schoolAllowed, const QStringList& extraProxies)
{
	QStringList allowed;
	for (const auto& domain : normalizeDomains(schoolAllowed))
	{
		if (isAlwaysBlocked(domain, extraProxies) == false)
		{
			allowed.append(domain);
		}
	}
	return uniqueSorted(allowed);
}


QStringList WebFilterLists::chromePolicyPatterns(const QStringList& domains)
{
	// Chrome/Edge URLBlocklist syntax is [scheme://][.]host[:port][/path]
	// A leading dot matches the host and every subdomain. Extension-style
	// *://*.host/* patterns are NOT valid here and make URLAllowlist miss.
	QStringList patterns;
	for (const auto& domain : normalizeDomains(domains))
	{
		patterns.append(domain);
		patterns.append(QLatin1Char('.') + domain);
	}
	return uniqueSorted(patterns);
}


QStringList WebFilterLists::chromeUrlPatterns(const QStringList& domains)
{
	return chromePolicyPatterns(domains);
}


QStringList WebFilterLists::browserAllowPatterns(const QStringList& domains)
{
	auto patterns = chromePolicyPatterns(domains);
	patterns.append({
		QStringLiteral("about:"),
		QStringLiteral("chrome://"),
		QStringLiteral("chrome-extension://"),
		QStringLiteral("chrome-search://"),
		QStringLiteral("devtools://"),
		QStringLiteral("edge://"),
		QStringLiteral("extension://"),
	});
	return uniqueSorted(patterns);
}


QStringList WebFilterLists::firefoxMatchPatterns(const QStringList& domains)
{
	QStringList patterns;
	for (const auto& domain : normalizeDomains(domains))
	{
		patterns.append(QStringLiteral("*://") + domain + QStringLiteral("/*"));
		patterns.append(QStringLiteral("*://*.") + domain + QStringLiteral("/*"));
	}
	return uniqueSorted(patterns);
}


QString WebFilterLists::proxyPacScript(bool whitelistMode,
									   const QStringList& domains,
									   const QStringList& extraBlocked)
{
	QString body;
	if (whitelistMode)
	{
		for (const auto& domain : normalizeDomains(domains))
		{
			if (isAlwaysBlocked(domain, extraBlocked))
			{
				continue;
			}
			const auto escaped = QString(domain).replace(QLatin1Char('\\'), QLatin1String("\\\\"))
									.replace(QLatin1Char('"'), QLatin1String("\\\""));
			body += QStringLiteral("  if (hostMatches(host, \"%1\")) return \"DIRECT\";\n").arg(escaped);
		}
		for (const auto& domain : uniqueSorted(hardcodedProxyDomains() + hardcodedDohDomains() +
											   normalizeDomains(extraBlocked)))
		{
			const auto escaped = QString(domain).replace(QLatin1Char('\\'), QLatin1String("\\\\"))
									.replace(QLatin1Char('"'), QLatin1String("\\\""));
			body += QStringLiteral("  if (hostMatches(host, \"%1\")) return \"PROXY 127.0.0.1:9\";\n").arg(escaped);
		}
		body += QStringLiteral("  return \"PROXY 127.0.0.1:9\";\n");
	}
	else
	{
		for (const auto& domain : uniqueSorted(hardcodedProxyDomains() + hardcodedDohDomains() +
											   normalizeDomains(extraBlocked) + normalizeDomains(domains)))
		{
			const auto escaped = QString(domain).replace(QLatin1Char('\\'), QLatin1String("\\\\"))
									.replace(QLatin1Char('"'), QLatin1String("\\\""));
			body += QStringLiteral("  if (hostMatches(host, \"%1\")) return \"PROXY 127.0.0.1:9\";\n").arg(escaped);
		}
		body += QStringLiteral("  return \"DIRECT\";\n");
	}

	return QStringLiteral(
			   "function hostMatches(host, domain) {\n"
			   "  host = host.toLowerCase();\n"
			   "  domain = domain.toLowerCase();\n"
			   "  return host === domain || dnsDomainIs(host, domain) || host.endsWith('.' + domain);\n"
			   "}\n"
			   "function FindProxyForURL(url, host) {\n"
			   "  if (!host) return \"DIRECT\";\n"
			   "  if (isPlainHostName(host) || host === \"localhost\") return \"DIRECT\";\n"
			   "  if (isInNet(host, \"127.0.0.0\", \"255.0.0.0\")) return \"DIRECT\";\n"
			   "  if (isInNet(host, \"10.0.0.0\", \"255.0.0.0\")) return \"DIRECT\";\n"
			   "  if (isInNet(host, \"172.16.0.0\", \"255.240.0.0\")) return \"DIRECT\";\n"
			   "  if (isInNet(host, \"192.168.0.0\", \"255.255.0.0\")) return \"DIRECT\";\n")
		   + body + QStringLiteral("}\n");
}


QStringList WebFilterLists::hostsNames(const QStringList& domains)
{
	QStringList names;
	for (const auto& domain : normalizeDomains(domains))
	{
		names.append(domain);
		if (domain.startsWith(QLatin1String("www.")) == false)
		{
			names.append(QStringLiteral("www.") + domain);
		}
	}
	return uniqueSorted(names);
}


QByteArray WebFilterLists::replaceHostsSection(const QByteArray& existing, const QStringList& domains)
{
	const auto without = removeHostsSection(existing);
	QByteArray section;
	section += QByteArrayLiteral("\n");
	section += QByteArray(HostsBeginMarker);
	section += QByteArrayLiteral("\n");
	for (const auto& name : hostsNames(domains))
	{
		section += QByteArrayLiteral("0.0.0.0 ");
		section += name.toUtf8();
		section += QByteArrayLiteral("\n");
		section += QByteArrayLiteral(":: ");
		section += name.toUtf8();
		section += QByteArrayLiteral("\n");
	}
	section += QByteArray(HostsEndMarker);
	section += QByteArrayLiteral("\n");

	auto result = without;
	if (result.isEmpty() == false && result.endsWith('\n') == false)
	{
		result += '\n';
	}
	result += section;
	return result;
}


bool WebFilterLists::hostsSectionPresent(const QByteArray& existing)
{
	return existing.contains(QByteArray(HostsBeginMarker));
}


QByteArray WebFilterLists::removeHostsSection(const QByteArray& existing)
{
	const QByteArray begin(HostsBeginMarker);
	const QByteArray end(HostsEndMarker);

	auto result = existing;
	while (true)
	{
		const auto start = result.indexOf(begin);
		if (start < 0)
		{
			break;
		}

		auto stop = result.indexOf(end, start);
		if (stop < 0)
		{
			result = result.left(start);
			break;
		}

		stop += end.size();
		while (stop < result.size() && (result.at(stop) == '\n' || result.at(stop) == '\r'))
		{
			++stop;
		}
		result.remove(start, stop - start);
	}

	while (result.endsWith('\n') && result.endsWith(QByteArrayLiteral("\n\n")))
	{
		result.chop(1);
	}

	return result;
}
