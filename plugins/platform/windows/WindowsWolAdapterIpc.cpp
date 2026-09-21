/*
 * WindowsWolAdapterIpc.cpp - SYSTEM-side adapter enable/disable helper
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <winsock2.h>
#include <windows.h>
#include <sddl.h>

#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QIODevice>

#include "VeyonCore.h"
#include "WindowsWolAdapterControl.h"
#include "WindowsWolAdapterIpc.h"


static constexpr auto SocketWaitTimeout = 3000;
static constexpr auto MessageTimeout = 8000;
static constexpr wchar_t PipePath[] = L"\\\\.\\pipe\\VeyonWolAdapterControl";


namespace
{

HANDLE createStopEvent()
{
	return CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

HANDLE createListeningPipe()
{
	PSECURITY_DESCRIPTOR securityDescriptor = nullptr;
	if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
			L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;AU)",
			SDDL_REVISION_1, &securityDescriptor, nullptr) == FALSE)
	{
		vWarning() << "can't create WOL helper security descriptor" << GetLastError();
		return INVALID_HANDLE_VALUE;
	}

	SECURITY_ATTRIBUTES securityAttributes{};
	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.lpSecurityDescriptor = securityDescriptor;
	securityAttributes.bInheritHandle = FALSE;

	const HANDLE pipe = CreateNamedPipeW(
		PipePath,
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
		1,
		256,
		256,
		1000,
		&securityAttributes);

	LocalFree(securityDescriptor);

	if (pipe == INVALID_HANDLE_VALUE)
	{
		vWarning() << "can't create WOL helper pipe" << GetLastError();
	}

	return pipe;
}

bool waitOverlapped(HANDLE handle, OVERLAPPED* overlapped, HANDLE stopEvent, DWORD timeoutMs, DWORD* bytesTransferred)
{
	HANDLE waitHandles[2] = { overlapped->hEvent, stopEvent };
	const DWORD waitCount = stopEvent ? 2 : 1;
	const DWORD waited = WaitForMultipleObjects(waitCount, waitHandles, FALSE, timeoutMs);
	if (waited == WAIT_OBJECT_0)
	{
		return GetOverlappedResult(handle, overlapped, bytesTransferred, FALSE) != FALSE;
	}

	CancelIoEx(handle, overlapped);
	GetOverlappedResult(handle, overlapped, bytesTransferred, FALSE);
	return false;
}

bool pipeTransfer(HANDLE pipe, HANDLE stopEvent, bool writing, void* buffer, DWORD size, DWORD timeoutMs)
{
	OVERLAPPED overlapped{};
	overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (overlapped.hEvent == nullptr)
	{
		return false;
	}

	DWORD transferred = 0;
	const BOOL started = writing
			? WriteFile(pipe, buffer, size, &transferred, &overlapped)
			: ReadFile(pipe, buffer, size, &transferred, &overlapped);

	bool ok = false;
	if (started)
	{
		ok = transferred == size;
	}
	else if (GetLastError() == ERROR_IO_PENDING)
	{
		ok = waitOverlapped(pipe, &overlapped, stopEvent, timeoutMs, &transferred) && transferred == size;
	}

	CloseHandle(overlapped.hEvent);
	return ok;
}

QString nativeDirectoryOf(const QString& path)
{
	wchar_t longPath[32768] = {};
	const DWORD longLength = GetLongPathNameW(reinterpret_cast<LPCWSTR>(path.utf16()),
											  longPath, DWORD(sizeof(longPath) / sizeof(longPath[0])));
	const QFileInfo info(longLength ? QString::fromWCharArray(longPath, int(longLength)) : path);
	const auto canonical = info.canonicalPath();
	return QDir::toNativeSeparators(canonical.isEmpty() ? info.absolutePath() : canonical);
}

bool isAuthorizedClient(HANDLE pipe)
{
	ULONG clientProcessId = 0;
	if (GetNamedPipeClientProcessId(pipe, &clientProcessId) == FALSE || clientProcessId == 0)
	{
		vWarning() << "can't determine WOL helper client process" << GetLastError();
		return false;
	}

	const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, clientProcessId);
	if (process == nullptr)
	{
		vWarning() << "can't open WOL helper client process" << clientProcessId << GetLastError();
		return false;
	}

	wchar_t clientPathBuffer[32768] = {};
	DWORD clientPathLength = DWORD(sizeof(clientPathBuffer) / sizeof(clientPathBuffer[0]));
	const bool gotClientPath =
			QueryFullProcessImageNameW(process, 0, clientPathBuffer, &clientPathLength) != FALSE;
	CloseHandle(process);

	if (gotClientPath == false)
	{
		vWarning() << "can't query WOL helper client image" << clientProcessId << GetLastError();
		return false;
	}

	wchar_t servicePathBuffer[32768] = {};
	const DWORD servicePathLength =
			GetModuleFileNameW(nullptr, servicePathBuffer, DWORD(sizeof(servicePathBuffer) / sizeof(servicePathBuffer[0])));
	if (servicePathLength == 0 || servicePathLength >= sizeof(servicePathBuffer) / sizeof(servicePathBuffer[0]))
	{
		vWarning() << "can't query Veyon Service image path" << GetLastError();
		return false;
	}

	const QString clientPath = QString::fromWCharArray(clientPathBuffer, int(clientPathLength));
	const QString servicePath = QString::fromWCharArray(servicePathBuffer, int(servicePathLength));
	const QFileInfo clientInfo(clientPath);
	const auto clientName = clientInfo.fileName().toLower();

	const bool knownExecutable =
			clientName == QStringLiteral("veyon-master.exe") ||
			clientName == QStringLiteral("veyon-cli.exe");
	const bool sameInstallDirectory =
			nativeDirectoryOf(clientPath).compare(nativeDirectoryOf(servicePath), Qt::CaseInsensitive) == 0;

	if (knownExecutable == false || sameInstallDirectory == false)
	{
		vWarning() << "rejected WOL helper client" << clientPath << "service" << servicePath;
		return false;
	}

	return true;
}

void handleConnectedClient(HANDLE pipe, HANDLE stopEvent)
{
	if (isAuthorizedClient(pipe) == false)
	{
		return;
	}

	QByteArray request(int(sizeof(quint32) * 3), 0);
	if (pipeTransfer(pipe, stopEvent, false, request.data(), DWORD(request.size()), MessageTimeout) == false)
	{
		vWarning() << "can't read WOL helper request" << GetLastError();
		return;
	}

	QDataStream in(request);
	in.setVersion(QDataStream::Qt_5_12);
	quint32 command = 0;
	quint32 interfaceIndex = 0;
	quint32 enabled = 0;
	in >> command >> interfaceIndex >> enabled;

	WindowsWolAdapterControl::dumpAdapters(QStringLiteral("helper request command %1 interface %2 enabled %3")
				.arg(command)
				.arg(interfaceIndex)
				.arg(enabled));

	quint32 result = 0;
	if (command == 1 &&
		WindowsWolAdapterControl::canTemporarilyEnableAdapter(interfaceIndex))
	{
		result = WindowsWolAdapterControl::setAdminStatusNative(interfaceIndex, enabled != 0) ? 1 : 0;
		WindowsWolAdapterControl::log(QStringLiteral("helper native %1 interface %2 enabled %3")
									  .arg(result)
									  .arg(interfaceIndex)
									  .arg(enabled));
	}
	else if (command == 1)
	{
		vWarning() << "rejected WOL helper request for non-Ethernet interface" << interfaceIndex;
	}

	QByteArray response;
	QDataStream out(&response, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_5_12);
	out << result;

	if (pipeTransfer(pipe, stopEvent, true, response.data(), DWORD(response.size()), MessageTimeout) == false)
	{
		vWarning() << "can't write WOL helper response" << GetLastError();
	}
}

}

WindowsWolAdapterIpcServer::WindowsWolAdapterIpcServer(QObject* parent) :
	QThread(parent),
	m_stopEvent(createStopEvent())
{
}



WindowsWolAdapterIpcServer::~WindowsWolAdapterIpcServer()
{
	if (m_stopEvent)
	{
		SetEvent(static_cast<HANDLE>(m_stopEvent));
	}
	wait();
	if (m_stopEvent)
	{
		CloseHandle(static_cast<HANDLE>(m_stopEvent));
		m_stopEvent = nullptr;
	}
}



QString WindowsWolAdapterIpcServer::serverName()
{
	return QStringLiteral("VeyonWolAdapterControl");
}



void WindowsWolAdapterIpcServer::run()
{
	const HANDLE stopEvent = static_cast<HANDLE>(m_stopEvent);
	if (stopEvent == nullptr)
	{
		vCritical() << "WOL helper stop event is missing";
		return;
	}

	const HANDLE pipe = createListeningPipe();
	if (pipe == INVALID_HANDLE_VALUE)
	{
		vCritical() << "WOL helper pipe is not available";
		WindowsWolAdapterControl::log(QStringLiteral("helper pipe create failed"));
		return;
	}

	vInfo() << "WOL adapter helper listening";
	WindowsWolAdapterControl::log(QStringLiteral("helper listening"));

	while (WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0)
	{
		OVERLAPPED overlapped{};
		overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		if (overlapped.hEvent == nullptr)
		{
			break;
		}

		DWORD unused = 0;
		const BOOL connected = ConnectNamedPipe(pipe, &overlapped);
		bool ready = connected == TRUE;
		if (ready == false)
		{
			const DWORD error = GetLastError();
			if (error == ERROR_PIPE_CONNECTED)
			{
				ready = true;
			}
			else if (error == ERROR_IO_PENDING)
			{
				ready = waitOverlapped(pipe, &overlapped, stopEvent, INFINITE, &unused);
			}
		}
		CloseHandle(overlapped.hEvent);

		if (WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if (ready)
		{
			handleConnectedClient(pipe, stopEvent);
			FlushFileBuffers(pipe);
			DisconnectNamedPipe(pipe);
		}
	}

	CloseHandle(pipe);
}



bool WindowsWolAdapterIpcClient::setAdminStatus(unsigned long interfaceIndex, bool enabled)
{
	HANDLE pipe = INVALID_HANDLE_VALUE;
	QElapsedTimer timer;
	timer.start();
	while (timer.elapsed() < SocketWaitTimeout)
	{
		pipe = CreateFileW(PipePath, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
						   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (pipe != INVALID_HANDLE_VALUE)
		{
			break;
		}

		const DWORD error = GetLastError();
		if (error == ERROR_PIPE_BUSY)
		{
			WaitNamedPipeW(PipePath, SocketWaitTimeout);
			continue;
		}
		if (error != ERROR_FILE_NOT_FOUND)
		{
			WindowsWolAdapterControl::log(QStringLiteral("helper CreateFile failed %1").arg(error));
			return false;
		}
		Sleep(50);
	}

	if (pipe == INVALID_HANDLE_VALUE)
	{
		vWarning() << "WOL adapter helper is not available" << GetLastError();
		WindowsWolAdapterControl::log(QStringLiteral("helper is not available error %1").arg(GetLastError()));
		return false;
	}

	DWORD mode = PIPE_READMODE_BYTE;
	SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);

	QByteArray payload;
	QDataStream out(&payload, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_5_12);
	out << quint32(1) << quint32(interfaceIndex) << quint32(enabled ? 1 : 0);

	DWORD written = 0;
	if (WriteFile(pipe, payload.constData(), DWORD(payload.size()), &written, nullptr) == FALSE ||
		written != DWORD(payload.size()))
	{
		WindowsWolAdapterControl::log(QStringLiteral("helper write failed %1").arg(GetLastError()));
		CloseHandle(pipe);
		return false;
	}

	QByteArray response(int(sizeof(quint32)), 0);
	DWORD read = 0;
	if (ReadFile(pipe, response.data(), DWORD(response.size()), &read, nullptr) == FALSE ||
		read != DWORD(response.size()))
	{
		WindowsWolAdapterControl::log(QStringLiteral("helper read failed %1").arg(GetLastError()));
		CloseHandle(pipe);
		return false;
	}
	CloseHandle(pipe);

	QDataStream in(response);
	in.setVersion(QDataStream::Qt_5_12);
	quint32 result = 0;
	in >> result;
	WindowsWolAdapterControl::log(QStringLiteral("helper result %1 for interface %2 enabled %3")
								  .arg(result)
								  .arg(interfaceIndex)
								  .arg(enabled));
	return result != 0;
}
