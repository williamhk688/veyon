/*
 * WebFilterSessionWatchdog.h - student-side monotonic expiry watchdog
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QElapsedTimer>
#include <QThread>
#include <QUuid>

#include "WebFilterSession.h"

class WebFilterSessionWatchdog : public QThread
{
	Q_OBJECT
public:
	explicit WebFilterSessionWatchdog(QObject* parent = nullptr);

	/*!
	 * One expiry pass. Safe to call from the service helper thread: the
	 * Windows service main thread does not run a Qt event loop, so a QTimer
	 * on that thread never fires.
	 */
	bool poll();

protected:
	void run() override;

private:
	void track(const WebFilterSession& session);
	void stopCurrent(WebFilterSession::StopReason reason);

	QElapsedTimer m_elapsed;
	QUuid m_trackedSessionId;
	qint64 m_baseElapsedMs = 0;
	qint64 m_lastCheckpointMs = 0;
};
