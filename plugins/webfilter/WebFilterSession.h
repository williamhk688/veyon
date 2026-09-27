/*
 * WebFilterSession.h - one timed classroom web-filter restriction
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUuid>

class WebFilterSession
{
public:
	enum class Mode
	{
		Off,
		Blacklist,
		Whitelist
	};

	enum class StopReason
	{
		TeacherManual,
		TeacherTimerExpired,
		ClientTimerExpired,
		RecoveryExpired,
		HardTTLExpired,
		EmergencyUnlock
	};

	QUuid sessionId;
	Mode mode = Mode::Off;
	qint64 startTimeMs = 0;
	qint64 durationMs = 0;
	qint64 expiresAtMs = 0;
	qint64 hardExpiresAtMs = 0;
	int version = 0;
	QStringList domains;
	qint64 checkpointElapsedMs = 0;
	qint64 checkpointWallMs = 0;
	qint64 checkpointTickMs = 0;

	bool isActive() const;
	QJsonObject toJson() const;
	QString toJsonText() const;

	static WebFilterSession fromJson(const QJsonObject& object, bool* ok = nullptr);
	static WebFilterSession fromJsonText(const QString& text, bool* ok = nullptr);
};
