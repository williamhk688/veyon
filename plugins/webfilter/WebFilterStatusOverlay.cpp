/*
 * WebFilterStatusOverlay.cpp - student-side badge, visible in Master thumbnails
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QExposeEvent>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScreen>
#include <QSurfaceFormat>

#include "WebFilterStatusOverlay.h"

static constexpr int OverlaySize = 56;
static constexpr int OverlayMargin = 16;

WebFilterStatusOverlay::WebFilterStatusOverlay() :
	QWindow(),
	m_backingStore(new QBackingStore(this))
{
	auto surfaceFormat = format();
	surfaceFormat.setAlphaBufferSize(8);
	setFormat(surfaceFormat);

	setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
			 Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
	resize(OverlaySize, OverlaySize);
	m_backingStore->resize(size());
	hide();
}

WebFilterStatusOverlay::~WebFilterStatusOverlay()
{
	delete m_backingStore;
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

	reposition();
	show();
	raise();
	requestUpdate();
}

bool WebFilterStatusOverlay::event(QEvent* event)
{
	if (event->type() == QEvent::UpdateRequest)
	{
		render();
		return true;
	}

	return QWindow::event(event);
}

void WebFilterStatusOverlay::exposeEvent(QExposeEvent*)
{
	if (isExposed())
	{
		render();
	}
}

void WebFilterStatusOverlay::resizeEvent(QResizeEvent* event)
{
	m_backingStore->resize(event->size());
	if (isExposed())
	{
		render();
	}
}

void WebFilterStatusOverlay::render()
{
	if (isExposed() == false)
	{
		return;
	}

	const QRect rect(0, 0, width(), height());
	m_backingStore->beginPaint(rect);

	QPainter painter(m_backingStore->paintDevice());
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.fillRect(rect, Qt::transparent);
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.setRenderHint(QPainter::Antialiasing);

	if (m_mode != PersistentWebFilterState::Mode::Off)
	{
		const bool whitelist = m_mode == PersistentWebFilterState::Mode::Whitelist;
		const QColor fill = whitelist ? QColor(15, 92, 166) : QColor(196, 86, 16);
		const QColor ring = whitelist ? QColor(232, 244, 255) : QColor(255, 236, 214);

		painter.setBrush(fill);
		painter.setPen(QPen(ring, 3));
		painter.drawEllipse(QRectF(3, 3, width() - 6, height() - 6));

		painter.setPen(QPen(Qt::white, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		const QRectF globe(14, 12, 28, 28);
		painter.setBrush(Qt::NoBrush);
		painter.drawEllipse(globe);
		painter.drawLine(QPointF(globe.center().x(), globe.top()), QPointF(globe.center().x(), globe.bottom()));
		painter.drawArc(QRectF(globe.left() + 7, globe.top(), 14, globe.height()), 90 * 16, 180 * 16);
		painter.drawArc(QRectF(globe.left() + 7, globe.top(), 14, globe.height()), 270 * 16, 180 * 16);
		painter.drawLine(QPointF(globe.left(), globe.center().y()), QPointF(globe.right(), globe.center().y()));

		if (whitelist)
		{
			QPainterPath check;
			check.moveTo(16, 40);
			check.lineTo(24, 47);
			check.lineTo(42, 28);
			painter.setPen(QPen(QColor(180, 255, 196), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			painter.drawPath(check);
		}
		else
		{
			painter.setPen(QPen(QColor(255, 220, 220), 4, Qt::SolidLine, Qt::RoundCap));
			painter.drawLine(QPointF(14, 14), QPointF(42, 42));
		}
	}

	painter.end();
	m_backingStore->endPaint();
	m_backingStore->flush(rect);
}

void WebFilterStatusOverlay::reposition()
{
	const auto* screen = QGuiApplication::primaryScreen();
	if (screen == nullptr)
	{
		return;
	}

	const auto geometry = screen->availableGeometry();
	setPosition(geometry.right() - width() - OverlayMargin,
				geometry.top() + OverlayMargin);
}
