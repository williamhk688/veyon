/*
 * WebFilterSessionStatusDialog.cpp - teacher view of the active session
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QDateTime>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>

#include "WebFilterSessionPolicy.h"
#include "WebFilterSessionStatusDialog.h"

WebFilterSessionStatusDialog::WebFilterSessionStatusDialog(QWidget* parent) :
	QDialog(parent)
{
	setWindowTitle(tr("網絡限制進行中 (Web restriction active)"));
	setModal(false);
	setWindowFlag(Qt::WindowContextHelpButtonHint, false);

	auto* layout = new QFormLayout(this);
	m_mode = new QLabel(this);
	m_remaining = new QLabel(this);
	m_endsAt = new QLabel(this);
	auto* restore = new QPushButton(tr("恢復網絡 (Restore web)"), this);
	layout->addRow(tr("模式 (Mode):"), m_mode);
	layout->addRow(tr("剩餘時間 (Remaining):"), m_remaining);
	layout->addRow(tr("結束於 (Ends at):"), m_endsAt);
	layout->addRow(restore);
	connect(restore, &QPushButton::clicked, this, &WebFilterSessionStatusDialog::restoreRequested);
}

void WebFilterSessionStatusDialog::setSession(const WebFilterSession& session)
{
	m_session = session;
	refresh();
}

void WebFilterSessionStatusDialog::refresh()
{
	if (m_session.isActive() == false)
	{
		hide();
		return;
	}

	const auto remaining = WebFilterSessionPolicy::remainingMs(
				m_session.durationMs,
				QDateTime::currentMSecsSinceEpoch() - m_session.startTimeMs,
				m_session.hardExpiresAtMs - m_session.startTimeMs);
	const auto minutes = remaining / 60000;
	const auto seconds = (remaining % 60000) / 1000;
	m_mode->setText(m_session.mode == WebFilterSession::Mode::Whitelist
					? tr("只允許白名單網站 (Allow whitelist sites only)")
					: tr("封鎖黑名單網站 (Block blacklist sites)"));
	m_remaining->setText(QStringLiteral("%1:%2").arg(minutes, 2, 10, QLatin1Char('0'))
						 .arg(seconds, 2, 10, QLatin1Char('0')));
	m_endsAt->setText(QDateTime::fromMSecsSinceEpoch(m_session.expiresAtMs).toString(QStringLiteral("HH:mm")));
}
