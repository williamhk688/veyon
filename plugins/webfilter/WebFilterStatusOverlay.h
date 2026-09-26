/*
 * WebFilterStatusOverlay.h - top-right badge while web filter is active
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QWidget>

#include "PersistentWebFilterState.h"

class WebFilterStatusOverlay : public QWidget
{
	Q_OBJECT
public:
	explicit WebFilterStatusOverlay(QWidget* parent = nullptr);

	void syncFromState();
	void setMode(PersistentWebFilterState::Mode mode);

protected:
	void paintEvent(QPaintEvent* event) override;
	void showEvent(QShowEvent* event) override;

private:
	void reposition();

	PersistentWebFilterState::Mode m_mode = PersistentWebFilterState::Mode::Off;
};
