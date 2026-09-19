/*
 * PersistentDemoState.h - persist teacher demo in HKLM (Windows)
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QByteArray>
#include <QRect>
#include <QString>

#include "Feature.h"

/*!
 * Windows 10/11 demo persistence.
 *
 * Production storage is HKLM (64-bit view):
 *   HKLM\SOFTWARE\Veyon Solutions\VeyonDemo
 *
 * Sibling of Veyon's LocalStore key so Configurator flushes cannot wipe it.
 *
 * On non-Windows builds every method is a no-op unless
 * VEYON_DEMO_STATE_FILE is set (unit tests only).
 */
class PersistentDemoState
{
public:
	struct Snapshot
	{
		Feature::Uid featureUid;
		QString demoServerHost;
		int demoServerPort{0};
		QByteArray demoAccessToken;
		QRect viewport;
		bool lockInput{false};

		bool isValid() const
		{
			return featureUid.isNull() == false;
		}
	};

	static Snapshot snapshot();
	static bool isActive();
	static bool lockInput();
	static bool setActive(const Snapshot& snapshot);
	static bool clear();

private:
	static Snapshot readSnapshot();
	static bool writeSnapshot(const Snapshot& snapshot);
};
