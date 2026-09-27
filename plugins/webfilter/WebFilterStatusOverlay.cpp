/*
 * WebFilterStatusOverlay.cpp - student-side badge, visible in Master thumbnails
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <algorithm>

#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QShortcut>
#include <QShowEvent>
#include <QTimer>

#include "FailsafeUnlock.h"
#include "VeyonCore.h"
#include "WebFilterStatusOverlay.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static constexpr int OverlaySize = 56;
static constexpr int OverlayMargin = 16;

#ifdef Q_OS_WIN
static WebFilterStatusOverlay* s_hookOverlay = nullptr;

static LRESULT CALLBACK webFilterUnlockHook(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION &&
		(wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) &&
		s_hookOverlay)
	{
		const auto* kbd = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
		const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
		const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
		const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		if (ctrl && alt && shift && kbd && kbd->vkCode == 'U')
		{
			QMetaObject::invokeMethod(s_hookOverlay, &WebFilterStatusOverlay::promptFailsafeUnlock,
									  Qt::QueuedConnection);
			return 1;
		}
	}
	return CallNextHookEx(nullptr, code, wParam, lParam);
}
#endif


WebFilterStatusOverlay::WebFilterStatusOverlay(QWidget* parent) :
	QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
	setAttribute(Qt::WA_ShowWithoutActivating);
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setFixedSize(OverlaySize, OverlaySize);
	auto* shortcut = new QShortcut(QKeySequence(QLatin1String(FailsafeUnlock::HotkeySequence)), this);
	shortcut->setContext(Qt::ApplicationShortcut);
	connect(shortcut, &QShortcut::activated, this, &WebFilterStatusOverlay::promptFailsafeUnlock);
	connect(&FailsafeHotkeyMonitor::instance(), &FailsafeHotkeyMonitor::hotkeyPressed,
			this, &WebFilterStatusOverlay::promptFailsafeUnlock);
	hide();
}


WebFilterStatusOverlay::~WebFilterStatusOverlay()
{
	unregisterUnlockHotkey();
	removeKeyboardHook();
}

void WebFilterStatusOverlay::syncFromState()
{
	setMode(PersistentWebFilterState::mode());
}

void WebFilterStatusOverlay::setMode(PersistentWebFilterState::Mode mode)
{
	m_mode = mode;
	if (m_mode == PersistentWebFilterState::Mode::Off)
	{
		unregisterUnlockHotkey();
		removeKeyboardHook();
		hide();
		return;
	}

	refreshTooltip();
	reposition();
	show();
	raise();
	registerUnlockHotkey();
	installKeyboardHook();
	update();
}

void WebFilterStatusOverlay::setRemainingMs(qint64 remainingMs, qint64 expiresAtMs)
{
	m_remainingMs = remainingMs;
	m_expiresAtMs = expiresAtMs;
	m_elapsed.restart();
	refreshTooltip();
	QTimer::singleShot(30000, this, &WebFilterStatusOverlay::refreshTooltip);
}

void WebFilterStatusOverlay::refreshTooltip()
{
	if (m_mode == PersistentWebFilterState::Mode::Off)
	{
		return;
	}

	const auto remaining = std::max<qint64>(0, m_remainingMs - m_elapsed.elapsed());
	const auto minutes = (remaining + 59999) / 60000;
	const auto modeText = m_mode == PersistentWebFilterState::Mode::Blacklist
			? tr("Web filter: blocking blacklist sites (網絡管制：正在封鎖黑名單網站)")
			: tr("Web filter: whitelist only (網絡管制：只允許白名單網站)");
	setToolTip(tr("%1\nRemaining: %2 minutes (剩餘約 %2 分鐘)").arg(modeText).arg(minutes));
}

void WebFilterStatusOverlay::registerUnlockHotkey()
{
#ifdef Q_OS_WIN
	unregisterUnlockHotkey();
	createWinId();
	if (RegisterHotKey(HWND(winId()), 1, MOD_CONTROL | MOD_ALT | MOD_SHIFT, 'U'))
	{
		m_hotkeyRegistered = true;
	}
	else
	{
		vWarning() << "RegisterHotKey failed for web-filter unlock";
	}
#else
	Q_UNUSED(this)
#endif
}


void WebFilterStatusOverlay::unregisterUnlockHotkey()
{
#ifdef Q_OS_WIN
	if (m_hotkeyRegistered)
	{
		UnregisterHotKey(HWND(winId()), 1);
		m_hotkeyRegistered = false;
	}
#endif
}


void WebFilterStatusOverlay::installKeyboardHook()
{
#ifdef Q_OS_WIN
	if (m_keyboardHook)
	{
		return;
	}
	s_hookOverlay = this;
	m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, webFilterUnlockHook, GetModuleHandleW(nullptr), 0);
	if (m_keyboardHook == nullptr)
	{
		s_hookOverlay = nullptr;
		vWarning() << "SetWindowsHookEx failed for web-filter unlock";
	}
#endif
}


void WebFilterStatusOverlay::removeKeyboardHook()
{
#ifdef Q_OS_WIN
	if (m_keyboardHook)
	{
		UnhookWindowsHookEx(static_cast<HHOOK>(m_keyboardHook));
		m_keyboardHook = nullptr;
	}
	if (s_hookOverlay == this)
	{
		s_hookOverlay = nullptr;
	}
#endif
}


void WebFilterStatusOverlay::promptFailsafeUnlock()
{
	if (m_failsafePromptOpen || m_mode == PersistentWebFilterState::Mode::Off)
	{
		return;
	}

	m_failsafePromptOpen = true;
	if (FailsafeUnlock::prompt(this))
	{
		Q_EMIT failsafeUnlocked();
		return;
	}
	m_failsafePromptOpen = false;
}


bool WebFilterStatusOverlay::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
	if (eventType == QByteArrayLiteral("windows_generic_MSG") ||
		eventType == QByteArrayLiteral("windows_dispatcher_MSG"))
	{
		const auto* msg = static_cast<MSG*>(message);
		if (msg && msg->message == WM_HOTKEY)
		{
			promptFailsafeUnlock();
			if (result)
			{
				*result = 0;
			}
			return true;
		}
	}
#endif
	return QWidget::nativeEvent(eventType, message, result);
}

void WebFilterStatusOverlay::paintEvent(QPaintEvent*)
{
	if (m_mode == PersistentWebFilterState::Mode::Off)
	{
		return;
	}

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	const bool whitelist = m_mode == PersistentWebFilterState::Mode::Whitelist;
	const QColor fill = whitelist ? QColor(16, 122, 72) : QColor(196, 48, 28);
	const QColor ring = whitelist ? QColor(220, 255, 230) : QColor(255, 220, 214);
	const auto icon = QPixmap(whitelist
							  ? QStringLiteral(":/webfilter/web-filter-allow.png")
							  : QStringLiteral(":/webfilter/web-filter-block.png"));

	painter.setBrush(fill);
	painter.setPen(QPen(ring, 3));
	painter.drawEllipse(QRectF(2, 2, width() - 4, height() - 4));

	if (icon.isNull() == false)
	{
		const QRect iconRect(10, 10, width() - 20, height() - 20);
		painter.drawPixmap(iconRect, icon);
	}
}

void WebFilterStatusOverlay::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	reposition();
}

void WebFilterStatusOverlay::reposition()
{
	const auto* screen = QGuiApplication::primaryScreen();
	if (screen == nullptr)
	{
		return;
	}

	const auto geometry = screen->availableGeometry();
	move(geometry.right() - width() - OverlayMargin,
		 geometry.top() + OverlayMargin);
}
