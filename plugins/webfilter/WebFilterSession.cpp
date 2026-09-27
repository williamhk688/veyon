/*
 * WebFilterSession.cpp - serialize one timed classroom restriction
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include "WebFilterSession.h"

bool WebFilterSession::isActive() const
{
	return mode != Mode::Off && sessionId.isNull() == false;
}

static QString modeToString(WebFilterSession::Mode mode)
{
	switch (mode)
	{
	case WebFilterSession::Mode::Blacklist:
		return QStringLiteral("Blacklist");
	case WebFilterSession::Mode::Whitelist:
		return QStringLiteral("Whitelist");
	case WebFilterSession::Mode::Off:
		break;
	}
	return QStringLiteral("Off");
}

static WebFilterSession::Mode modeFromString(const QString& text)
{
	if (text.compare(QLatin1String("Blacklist"), Qt::CaseInsensitive) == 0)
	{
		return WebFilterSession::Mode::Blacklist;
	}
	if (text.compare(QLatin1String("Whitelist"), Qt::CaseInsensitive) == 0 ||
		text.compare(QLatin1String("WhitelistOnly"), Qt::CaseInsensitive) == 0)
	{
		return WebFilterSession::Mode::Whitelist;
	}
	return WebFilterSession::Mode::Off;
}

QJsonObject WebFilterSession::toJson() const
{
	QJsonObject object;
	object.insert(QStringLiteral("sessionId"), sessionId.toString(QUuid::WithoutBraces));
	object.insert(QStringLiteral("mode"), modeToString(mode));
	object.insert(QStringLiteral("startTimeMs"), startTimeMs);
	object.insert(QStringLiteral("durationMs"), durationMs);
	object.insert(QStringLiteral("expiresAtMs"), expiresAtMs);
	object.insert(QStringLiteral("hardExpiresAtMs"), hardExpiresAtMs);
	object.insert(QStringLiteral("version"), version);
	object.insert(QStringLiteral("checkpointElapsedMs"), checkpointElapsedMs);
	object.insert(QStringLiteral("checkpointWallMs"), checkpointWallMs);
	object.insert(QStringLiteral("checkpointTickMs"), checkpointTickMs);
	QJsonArray domainArray;
	for (const auto& domain : domains)
	{
		domainArray.append(domain);
	}
	object.insert(QStringLiteral("domains"), domainArray);
	return object;
}

QString WebFilterSession::toJsonText() const
{
	return QString::fromUtf8(QJsonDocument(toJson()).toJson(QJsonDocument::Compact));
}

WebFilterSession WebFilterSession::fromJson(const QJsonObject& object, bool* ok)
{
	WebFilterSession session;
	if (object.isEmpty())
	{
		if (ok)
		{
			*ok = false;
		}
		return session;
	}

	session.sessionId = QUuid::fromString(object.value(QStringLiteral("sessionId")).toString());
	if (object.contains(QStringLiteral("mode")) == false)
	{
		if (ok)
		{
			*ok = false;
		}
		return {};
	}
	session.mode = modeFromString(object.value(QStringLiteral("mode")).toString());
	session.startTimeMs = qint64(object.value(QStringLiteral("startTimeMs")).toDouble());
	session.durationMs = qint64(object.value(QStringLiteral("durationMs")).toDouble());
	session.expiresAtMs = qint64(object.value(QStringLiteral("expiresAtMs")).toDouble());
	session.hardExpiresAtMs = qint64(object.value(QStringLiteral("hardExpiresAtMs")).toDouble());
	session.version = object.value(QStringLiteral("version")).toInt();
	session.checkpointElapsedMs = qint64(object.value(QStringLiteral("checkpointElapsedMs")).toDouble());
	session.checkpointWallMs = qint64(object.value(QStringLiteral("checkpointWallMs")).toDouble());
	session.checkpointTickMs = qint64(object.value(QStringLiteral("checkpointTickMs")).toDouble());
	const auto domainArray = object.value(QStringLiteral("domains")).toArray();
	for (const auto& value : domainArray)
	{
		session.domains.append(value.toString());
	}
	if (ok)
	{
		*ok = object.contains(QStringLiteral("sessionId")) &&
			  object.contains(QStringLiteral("durationMs")) &&
			  object.contains(QStringLiteral("startTimeMs"));
	}
	return session;
}

WebFilterSession WebFilterSession::fromJsonText(const QString& text, bool* ok)
{
	if (text.trimmed().isEmpty())
	{
		if (ok)
		{
			*ok = false;
		}
		return {};
	}

	QJsonParseError error{};
	const auto document = QJsonDocument::fromJson(text.toUtf8(), &error);
	if (error.error != QJsonParseError::NoError || document.isObject() == false)
	{
		if (ok)
		{
			*ok = false;
		}
		return {};
	}

	return fromJson(document.object(), ok);
}
