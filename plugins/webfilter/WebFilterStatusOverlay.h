/*
 * WebFilterStatusOverlay.h - top-right badge while web filter is active
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QElapsedTimer>
#include <QWidget>

#include "PersistentWebFilterState.h"

// QWidget is only safe in veyon-worker (QApplication). Never construct this
// in veyon-server — that process is QGuiApplication-only and will abort.
class WebFilterStatusOverlay : public QWidget
{
	Q_OBJECT
public:
	explicit WebFilterStatusOverlay(QWidget* parent = nullptr);

	void syncFromState();
	void setMode(PersistentWebFilterState::Mode mode);
	void setRemainingMs(qint64 remainingMs, qint64 expiresAtMs);

Q_SIGNALS:
	void failsafeUnlocked();

protected:
	void paintEvent(QPaintEvent* event) override;
	void showEvent(QShowEvent* event) override;

private:
	void reposition();
	void promptFailsafeUnlock();
	void refreshTooltip();

	PersistentWebFilterState::Mode m_mode = PersistentWebFilterState::Mode::Off;
	qint64 m_remainingMs = 0;
	qint64 m_expiresAtMs = 0;
	QElapsedTimer m_elapsed;
	bool m_failsafePromptOpen = false;
};
