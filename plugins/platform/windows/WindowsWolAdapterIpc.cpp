/*
 * WindowsWolAdapterIpc.cpp - SYSTEM-side adapter enable/disable helper
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QDataStream>
#include <QElapsedTimer>
#include <QIODevice>
#include <QLocalServer>
#include <QLocalSocket>

#include "VeyonCore.h"
#include "WindowsWolAdapterControl.h"
#include "WindowsWolAdapterIpc.h"


static constexpr auto SocketWaitTimeout = 1000;
static constexpr auto MessageTimeout = 8000;


WindowsWolAdapterIpcServer::WindowsWolAdapterIpcServer(QObject* parent) :
	QThread(parent)
{
	start();
}



WindowsWolAdapterIpcServer::~WindowsWolAdapterIpcServer()
{
	quit();
	wait();
}



QString WindowsWolAdapterIpcServer::serverName()
{
	return QStringLiteral("VeyonWolAdapterControl");
}



void WindowsWolAdapterIpcServer::run()
{
	QLocalServer::removeServer(serverName());

	m_server = new QLocalServer;
	m_server->setSocketOptions(QLocalServer::WorldAccessOption);

	if (m_server->listen(serverName()) == false)
	{
		vCritical() << "can't listen" << m_server->errorString();
		delete m_server;
		m_server = nullptr;
		return;
	}

	connect(m_server, &QLocalServer::newConnection, m_server, [this]() { acceptConnection(); });

	QThread::run();

	delete m_server;
	m_server = nullptr;
}



void WindowsWolAdapterIpcServer::acceptConnection()
{
	auto socket = m_server->nextPendingConnection();
	if (socket == nullptr)
	{
		return;
	}

	connect(socket, &QLocalSocket::readyRead, socket, [socket]() {
		QDataStream in(socket);
		in.setVersion(QDataStream::Qt_5_12);
		if (socket->bytesAvailable() < qint64(sizeof(quint32) * 3))
		{
			return;
		}

		quint32 command = 0;
		quint32 interfaceIndex = 0;
		quint32 enabled = 0;
		in >> command >> interfaceIndex >> enabled;

		quint32 result = 0;
		if (command == 1)
		{
			result = WindowsWolAdapterControl::setAdminStatusNative(interfaceIndex, enabled != 0) ? 1 : 0;
		}

		QByteArray payload;
		QDataStream out(&payload, QIODevice::WriteOnly);
		out.setVersion(QDataStream::Qt_5_12);
		out << result;
		socket->write(payload);
		socket->flush();
		socket->disconnectFromServer();
	});
}



bool WindowsWolAdapterIpcClient::setAdminStatus(unsigned long interfaceIndex, bool enabled)
{
	QLocalSocket socket;
	socket.connectToServer(WindowsWolAdapterIpcServer::serverName());
	if (socket.waitForConnected(SocketWaitTimeout) == false)
	{
		vWarning() << "WOL adapter helper is not available:" << socket.errorString();
		return false;
	}

	QByteArray payload;
	QDataStream out(&payload, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_5_12);
	out << quint32(1) << quint32(interfaceIndex) << quint32(enabled ? 1 : 0);
	socket.write(payload);
	socket.flush();

	QElapsedTimer timeout;
	timeout.start();
	while (timeout.elapsed() < MessageTimeout && socket.bytesAvailable() < qint64(sizeof(quint32)))
	{
		socket.waitForReadyRead(SocketWaitTimeout);
	}

	if (socket.bytesAvailable() < qint64(sizeof(quint32)))
	{
		vWarning() << "no response from WOL adapter helper";
		return false;
	}

	QDataStream in(&socket);
	in.setVersion(QDataStream::Qt_5_12);
	quint32 result = 0;
	in >> result;
	return result != 0;
}
