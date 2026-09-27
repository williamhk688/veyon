/*
 * WebFilterStatusOverlay.h - top-right badge while web filter is active
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QBackingStore>
#include <QWindow>

#include "PersistentWebFilterState.h"

// QWindow, not QWidget: veyon-server is a QGuiApplication and will abort if a
// QWidget is created ("Cannot create a QWidget without QApplication").
class WebFilterStatusOverlay : public QWindow
{
	Q_OBJECT
public:
	explicit WebFilterStatusOverlay();
	~WebFilterStatusOverlay() override;

	void syncFromState();
	void setMode(PersistentWebFilterState::Mode mode);

protected:
	bool event(QEvent* event) override;
	void exposeEvent(QExposeEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;

private:
	void render();
	void reposition();

	QBackingStore* m_backingStore = nullptr;
	PersistentWebFilterState::Mode m_mode = PersistentWebFilterState::Mode::Off;
};
