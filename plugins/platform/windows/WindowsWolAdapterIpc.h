/*
 * WindowsWolAdapterIpc.h - local IPC so Master can ask Veyon Service to
 * enable or disable a network adapter without a UAC prompt
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QThread>

class WindowsWolAdapterIpcServer : public QThread
{
	Q_OBJECT
public:
	explicit WindowsWolAdapterIpcServer(QObject* parent = nullptr);
	~WindowsWolAdapterIpcServer() override;

	static QString serverName();

protected:
	void run() override;

private:
	void* m_stopEvent{nullptr};
};

class WindowsWolAdapterIpcClient
{
public:
	static bool setAdminStatus(unsigned long interfaceIndex, bool enabled);
};
