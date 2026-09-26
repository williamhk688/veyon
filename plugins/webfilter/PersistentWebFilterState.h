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

class PersistentWebFilterState
{
public:
	enum class Mode
	{
		Off,
		Blacklist,
		Whitelist
	};

	static Mode mode();
	static QStringList domains();
	static QString policySnapshot();
	static bool setBlacklist(const QStringList& domains);
	static bool setWhitelist(const QStringList& domains);
	static bool setPolicySnapshot(const QString& snapshot);
	static bool clear();

private:
	static Mode readMode();
	static bool writeMode(Mode mode);
	static QStringList readDomains();
	static bool writeDomains(const QStringList& domains);
	static QString readSnapshot();
	static bool writeSnapshot(const QString& snapshot);
};
