/*
 * PersistentWebFilterState.h - persist blacklist until teacher restores
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QString>
#include <QStringList>

#include "WebFilterSession.h"

class PersistentWebFilterState
{
public:
	using Mode = WebFilterSession::Mode;

	static Mode mode();
	static QStringList domains();
	static QString policySnapshot();
	static WebFilterSession session();
	static bool setBlacklist(const QStringList& domains);
	static bool setWhitelist(const QStringList& domains);
	static bool setPolicySnapshot(const QString& snapshot);
	static bool saveSession(const WebFilterSession& session);
	static bool updateCheckpoint(qint64 elapsedMs, qint64 wallMs);
	static bool noteEmergencyUnlocked(const QUuid& sessionId);
	static QUuid emergencyUnlockedSession();
	static bool isEmergencyUnlocked(const QUuid& sessionId);
	static bool shouldIgnoreApply(const QUuid& incomingSessionId);
	static bool clearEmergencyUnlocked();
	static bool clear();

private:
	static Mode readMode();
	static bool writeMode(Mode mode);
	static QStringList readDomains();
	static bool writeDomains(const QStringList& domains);
	static QString readSnapshot();
	static bool writeSnapshot(const QString& snapshot);
};
