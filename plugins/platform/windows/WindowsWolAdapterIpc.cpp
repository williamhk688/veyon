/*
 * WindowsWolAdapterIpc.cpp - SYSTEM-side adapter enable/disable helper
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <windows.h>

#include <QDataStream>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QIODevice>
#include <QLocalServer>
#include <QLocalSocket>

#include "VeyonCore.h"
#include "WindowsWolAdapterControl.h"
#include "WindowsWolAdapterIpc.h"


static constexpr auto SocketWaitTimeout = 1000;
static constexpr auto MessageTimeout = 8000;


namespace
{

bool isAuthorizedClient(QLocalSocket* socket)
{
	if (socket == nullptr || socket->socketDescriptor() == -1)
	{
		return false;
	}

	ULONG clientProcessId = 0;
	const auto pipeHandle = reinterpret_cast<HANDLE>(socket->socketDescriptor());
	if (GetNamedPipeClientProcessId(pipeHandle, &clientProcessId) == FALSE || clientProcessId == 0)
	{
		vWarning() << "can't determine WOL helper client process";
		return false;
	}

	const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, clientProcessId);
	if (process == nullptr)
	{
		vWarning() << "can't open WOL helper client process" << clientProcessId;
		return false;
	}

	wchar_t clientPathBuffer[32768] = {};
	DWORD clientPathLength = DWORD(sizeof(clientPathBuffer) / sizeof(clientPathBuffer[0]));
	const bool gotClientPath =
			QueryFullProcessImageNameW(process, 0, clientPathBuffer, &clientPathLength) != FALSE;
	CloseHandle(process);

	if (gotClientPath == false)
	{
		vWarning() << "can't query WOL helper client image" << clientProcessId;
		return false;
	}

	wchar_t servicePathBuffer[32768] = {};
	const DWORD servicePathLength =
			GetModuleFileNameW(nullptr, servicePathBuffer, DWORD(sizeof(servicePathBuffer) / sizeof(servicePathBuffer[0])));
	if (servicePathLength == 0 || servicePathLength >= sizeof(servicePathBuffer) / sizeof(servicePathBuffer[0]))
	{
		vWarning() << "can't query Veyon Service image path";
		return false;
	}

	const QFileInfo clientInfo(QString::fromWCharArray(clientPathBuffer, int(clientPathLength)));
	const QFileInfo serviceInfo(QString::fromWCharArray(servicePathBuffer, int(servicePathLength)));
	const auto clientName = clientInfo.fileName().toLower();

	const bool knownExecutable =
			clientName == QStringLiteral("veyon-master.exe") ||
			clientName == QStringLiteral("veyon-cli.exe");
	const bool sameInstallDirectory =
			clientInfo.absolutePath().compare(serviceInfo.absolutePath(), Qt::CaseInsensitive) == 0;

	if (knownExecutable == false || sameInstallDirectory == false)
	{
		vWarning() << "rejected WOL helper client" << clientInfo.absoluteFilePath();
		return false;
	}

	return true;
}

}


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
		if (isAuthorizedClient(socket) == false)
		{
			socket->disconnectFromServer();
			return;
		}

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
		if (command == 1 &&
			WindowsWolAdapterControl::canTemporarilyEnableAdapter(interfaceIndex))
		{
			result = WindowsWolAdapterControl::setAdminStatusNative(interfaceIndex, enabled != 0) ? 1 : 0;
		}
		else if (command == 1)
		{
			vWarning() << "rejected WOL helper request for non-Ethernet interface" << interfaceIndex;
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
