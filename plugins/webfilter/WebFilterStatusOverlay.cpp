/*
 * WebFilterStatusOverlay.cpp - student-side badge, visible in Master thumbnails
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QShowEvent>

#include "WebFilterStatusOverlay.h"

static constexpr int OverlaySize = 56;
static constexpr int OverlayMargin = 16;

WebFilterStatusOverlay::WebFilterStatusOverlay(QWidget* parent) :
	QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus)
{
	setAttribute(Qt::WA_ShowWithoutActivating);
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setFixedSize(OverlaySize, OverlaySize);
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

	if (m_mode == PersistentWebFilterState::Mode::Blacklist)
	{
		setToolTip(tr("網絡管制：正在封鎖黑名單網站 (Blocking blacklist sites)"));
	}
	else
	{
		setToolTip(tr("網絡管制：只允許白名單網站 (Allowing whitelist sites only)"));
	}

	reposition();
	show();
	raise();
	update();
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
