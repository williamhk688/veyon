/*
 * WebFilterDurationDialog.cpp - choose how long a web-filter session lasts
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>

#include "WebFilterDurationDialog.h"
#include "WebFilterSessionPolicy.h"

WebFilterDurationDialog::WebFilterDurationDialog(QWidget* parent, qint64 maxTtlMs) :
	QDialog(parent),
	m_maxTtlMs(maxTtlMs > 0 ? maxTtlMs : WebFilterSessionPolicy::DefaultMaxTtlMs)
{
	setWindowTitle(tr("網絡限制時長 (Web restriction duration)"));
	setModal(true);

	auto* layout = new QFormLayout(this);
	m_choice = new QComboBox(this);
	m_choice->addItem(tr("10 分鐘 (10 minutes)"), int(Choice::Minutes10));
	m_choice->addItem(tr("20 分鐘 (20 minutes)"), int(Choice::Minutes20));
	m_choice->addItem(tr("30 分鐘 (30 minutes)"), int(Choice::Minutes30));
	m_choice->addItem(tr("45 分鐘 (45 minutes)"), int(Choice::Minutes45));
	m_choice->addItem(tr("60 分鐘 (60 minutes)"), int(Choice::Minutes60));
	m_choice->addItem(tr("自訂時長 (Custom duration)"), int(Choice::CustomDuration));
	m_choice->addItem(tr("自訂結束時間 (Custom end time)"), int(Choice::CustomEndTime));
	m_choice->setCurrentIndex(2);

	m_customMinutes = new QSpinBox(this);
	m_customMinutes->setRange(1, int(m_maxTtlMs / 60000));
	m_customMinutes->setValue(30);
	m_customMinutes->setSuffix(tr(" 分鐘 (min)"));

	m_customEnd = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(30 * 60), this);
	m_customEnd->setDisplayFormat(QStringLiteral("HH:mm"));
	m_customEnd->setCalendarPopup(false);

	m_endsAt = new QLabel(this);

	layout->addRow(tr("鎖定時長 (Duration):"), m_choice);
	layout->addRow(tr("自訂分鐘 (Custom minutes):"), m_customMinutes);
	layout->addRow(tr("自訂結束 (Custom end):"), m_customEnd);
	layout->addRow(tr("結束於 (Ends at):"), m_endsAt);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	layout->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_choice, qOverload<int>(&QComboBox::currentIndexChanged), this, &WebFilterDurationDialog::updateSummary);
	connect(m_customMinutes, qOverload<int>(&QSpinBox::valueChanged), this, &WebFilterDurationDialog::updateSummary);
	connect(m_customEnd, &QDateTimeEdit::dateTimeChanged, this, &WebFilterDurationDialog::updateSummary);
	updateSummary();
}

qint64 WebFilterDurationDialog::durationMs() const
{
	return selectedDurationMs();
}

qint64 WebFilterDurationDialog::selectedDurationMs() const
{
	const auto choice = Choice(m_choice->currentData().toInt());
	qint64 minutes = 30;
	switch (choice)
	{
	case Choice::Minutes10:
		minutes = 10;
		break;
	case Choice::Minutes20:
		minutes = 20;
		break;
	case Choice::Minutes30:
		minutes = 30;
		break;
	case Choice::Minutes45:
		minutes = 45;
		break;
	case Choice::Minutes60:
		minutes = 60;
		break;
	case Choice::CustomDuration:
		minutes = m_customMinutes->value();
		break;
	case Choice::CustomEndTime:
		return WebFilterSessionPolicy::durationUntilEndMs(
					QDateTime::currentMSecsSinceEpoch(),
					m_customEnd->dateTime().toMSecsSinceEpoch(),
					m_maxTtlMs);
	}
	return WebFilterSessionPolicy::clampDurationMs(minutes * 60 * 1000, m_maxTtlMs);
}

void WebFilterDurationDialog::updateSummary()
{
	const auto custom = Choice(m_choice->currentData().toInt());
	m_customMinutes->setVisible(custom == Choice::CustomDuration);
	m_customEnd->setVisible(custom == Choice::CustomEndTime);
	const auto end = QDateTime::currentDateTime().addMSecs(selectedDurationMs());
	m_endsAt->setText(end.toString(QStringLiteral("HH:mm")));
}
