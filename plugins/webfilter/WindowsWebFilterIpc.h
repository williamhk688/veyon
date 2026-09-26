/*
 * WindowsWebFilterIpc.h - SYSTEM helper for hosts and browser policies
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#ifdef Q_OS_WIN

#include <QStringList>
#include <QThread>

class WindowsWebFilterIpcServer : public QThread
{
	Q_OBJECT
public:
	explicit WindowsWebFilterIpcServer(QObject* parent = nullptr);
	~WindowsWebFilterIpcServer() override;

	void run() override;

private:
	void* m_stopEvent = nullptr;
};

class WindowsWebFilterIpcClient
{
public:
	enum class Command : quint32
	{
		Blacklist = 1,
		Whitelist = 2,
		Restore = 3,
		Reconcile = 4
	};

	static bool request(Command command, const QStringList& domains = {});
};

#endif
