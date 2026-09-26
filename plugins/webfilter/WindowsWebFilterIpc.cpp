/*
 * WindowsWebFilterIpc.cpp - named pipe between Veyon Server and Service
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
#include "WebFilterEngine.h"
#include "WindowsWebFilterIpc.h"

static constexpr auto SocketWaitTimeout = 3000;
static constexpr auto MessageTimeout = 15000;
static constexpr wchar_t PipePath[] = L"\\\\.\\pipe\\VeyonWebFilterControl";

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
		vWarning() << "can't create web filter security descriptor" << GetLastError();
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
		65536,
		65536,
		1000,
		&securityAttributes);

	LocalFree(securityDescriptor);

	if (pipe == INVALID_HANDLE_VALUE)
	{
		vWarning() << "can't create web filter pipe" << GetLastError();
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
		vWarning() << "can't determine web filter client process" << GetLastError();
		return false;
	}

	const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, clientProcessId);
	if (process == nullptr)
	{
		vWarning() << "can't open web filter client process" << clientProcessId << GetLastError();
		return false;
	}

	wchar_t clientPathBuffer[32768] = {};
	DWORD clientPathLength = DWORD(sizeof(clientPathBuffer) / sizeof(clientPathBuffer[0]));
	const bool gotClientPath =
			QueryFullProcessImageNameW(process, 0, clientPathBuffer, &clientPathLength) != FALSE;
	CloseHandle(process);

	if (gotClientPath == false)
	{
		return false;
	}

	wchar_t servicePathBuffer[32768] = {};
	const DWORD servicePathLength =
			GetModuleFileNameW(nullptr, servicePathBuffer, DWORD(sizeof(servicePathBuffer) / sizeof(servicePathBuffer[0])));
	if (servicePathLength == 0 || servicePathLength >= sizeof(servicePathBuffer) / sizeof(servicePathBuffer[0]))
	{
		return false;
	}

	const QString clientPath = QString::fromWCharArray(clientPathBuffer, int(clientPathLength));
	const QString servicePath = QString::fromWCharArray(servicePathBuffer, int(servicePathLength));
	const QFileInfo clientInfo(clientPath);
	const auto clientName = clientInfo.fileName().toLower();
	const bool knownExecutable = clientName == QStringLiteral("veyon-server.exe");
	const bool sameInstallDirectory =
			nativeDirectoryOf(clientPath).compare(nativeDirectoryOf(servicePath), Qt::CaseInsensitive) == 0;

	if (knownExecutable == false || sameInstallDirectory == false)
	{
		vWarning() << "rejected web filter client" << clientPath;
		return false;
	}

	return true;
}

QByteArray encodeRequest(WindowsWebFilterIpcClient::Command command, const QStringList& domains)
{
	QByteArray payload;
	QDataStream out(&payload, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_5_12);
	out << quint32(command) << domains;
	return payload;
}

bool decodeRequest(const QByteArray& payload, WindowsWebFilterIpcClient::Command* command, QStringList* domains)
{
	QDataStream in(payload);
	in.setVersion(QDataStream::Qt_5_12);
	quint32 raw = 0;
	in >> raw >> *domains;
	if (in.status() != QDataStream::Ok)
	{
		return false;
	}
	*command = WindowsWebFilterIpcClient::Command(raw);
	return true;
}

bool applyCommand(WindowsWebFilterIpcClient::Command command, const QStringList& domains)
{
	switch (command)
	{
	case WindowsWebFilterIpcClient::Command::Blacklist:
		return WebFilterEngine::applyBlacklist(domains);
	case WindowsWebFilterIpcClient::Command::Whitelist:
		return WebFilterEngine::applyWhitelist(domains);
	case WindowsWebFilterIpcClient::Command::Restore:
		return WebFilterEngine::restore();
	case WindowsWebFilterIpcClient::Command::Reconcile:
		return WebFilterEngine::reconcileOnServiceStart();
	}
	return false;
}

void handleConnectedClient(HANDLE pipe, HANDLE stopEvent)
{
	if (isAuthorizedClient(pipe) == false)
	{
		return;
	}

	quint32 size = 0;
	if (pipeTransfer(pipe, stopEvent, false, &size, sizeof(size), MessageTimeout) == false || size == 0 || size > 60000)
	{
		vWarning() << "can't read web filter request size";
		return;
	}

	QByteArray request(int(size), 0);
	if (pipeTransfer(pipe, stopEvent, false, request.data(), size, MessageTimeout) == false)
	{
		vWarning() << "can't read web filter request";
		return;
	}

	WindowsWebFilterIpcClient::Command command = WindowsWebFilterIpcClient::Command::Restore;
	QStringList domains;
	quint32 result = 0;
	if (decodeRequest(request, &command, &domains))
	{
		result = applyCommand(command, domains) ? 1 : 0;
	}

	if (pipeTransfer(pipe, stopEvent, true, &result, sizeof(result), MessageTimeout) == false)
	{
		vWarning() << "can't write web filter response";
	}
}

}

WindowsWebFilterIpcServer::WindowsWebFilterIpcServer(QObject* parent) :
	QThread(parent),
	m_stopEvent(createStopEvent())
{
}

WindowsWebFilterIpcServer::~WindowsWebFilterIpcServer()
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

void WindowsWebFilterIpcServer::run()
{
	const HANDLE stopEvent = static_cast<HANDLE>(m_stopEvent);
	if (stopEvent == nullptr)
	{
		return;
	}

	const HANDLE pipe = createListeningPipe();
	if (pipe == INVALID_HANDLE_VALUE)
	{
		return;
	}

	vInfo() << "web filter helper listening";

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

bool WindowsWebFilterIpcClient::request(Command command, const QStringList& domains)
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
			vWarning() << "web filter CreateFile failed" << error;
			return false;
		}
		Sleep(50);
	}

	if (pipe == INVALID_HANDLE_VALUE)
	{
		vWarning() << "web filter helper is not available";
		return false;
	}

	DWORD mode = PIPE_READMODE_BYTE;
	SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);

	const auto payload = encodeRequest(command, domains);
	const auto size = quint32(payload.size());
	DWORD written = 0;
	if (WriteFile(pipe, &size, sizeof(size), &written, nullptr) == FALSE || written != sizeof(size) ||
		WriteFile(pipe, payload.constData(), DWORD(payload.size()), &written, nullptr) == FALSE ||
		written != DWORD(payload.size()))
	{
		vWarning() << "web filter write failed" << GetLastError();
		CloseHandle(pipe);
		return false;
	}

	quint32 result = 0;
	DWORD read = 0;
	if (ReadFile(pipe, &result, sizeof(result), &read, nullptr) == FALSE || read != sizeof(result))
	{
		vWarning() << "web filter read failed" << GetLastError();
		CloseHandle(pipe);
		return false;
	}
	CloseHandle(pipe);
	return result != 0;
}
