/*
 * WebFilterSessionStatusDialog.h - teacher view of the active session
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QDialog>

#include "WebFilterSession.h"

class QLabel;

class WebFilterSessionStatusDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WebFilterSessionStatusDialog(QWidget* parent = nullptr);

	void setSession(const WebFilterSession& session);

Q_SIGNALS:
	void restoreRequested();

private:
	void refresh();

	WebFilterSession m_session;
	QLabel* m_mode = nullptr;
	QLabel* m_remaining = nullptr;
	QLabel* m_endsAt = nullptr;
};
