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
#include "WebFilterStatusOverlay.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static constexpr int OverlaySize = 56;
static constexpr int OverlayMargin = 16;

WebFilterStatusOverlay::WebFilterStatusOverlay(QWidget* parent) :
	QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus)
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

void WebFilterStatusOverlay::syncFromState()
{
	setMode(PersistentWebFilterState::mode());
}

void WebFilterStatusOverlay::setMode(PersistentWebFilterState::Mode mode)
{
	m_mode = mode;
	if (m_mode == PersistentWebFilterState::Mode::Off)
	{
		hide();
		return;
	}

	refreshTooltip();
	reposition();
	show();
	raise();
#ifdef Q_OS_WIN
	RegisterHotKey(HWND(winId()), 1, MOD_CONTROL | MOD_ALT | MOD_SHIFT, 'U');
#endif
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
