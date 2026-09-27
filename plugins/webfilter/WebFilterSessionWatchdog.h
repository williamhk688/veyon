/*
 * WebFilterSessionWatchdog.h - student-side monotonic expiry watchdog
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUuid>

#include "WebFilterSession.h"

class WebFilterSessionWatchdog : public QObject
{
	Q_OBJECT
public:
	explicit WebFilterSessionWatchdog(QObject* parent = nullptr);

	void start();

private:
	void tick();
	void track(const WebFilterSession& session);
	void stopCurrent(WebFilterSession::StopReason reason);

	QTimer m_timer{this};
	QElapsedTimer m_elapsed;
	QUuid m_trackedSessionId;
	qint64 m_baseElapsedMs = 0;
	qint64 m_lastCheckpointMs = 0;
};
