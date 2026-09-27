/*
 * WebFilterDurationDialog.h - choose how long a web-filter session lasts
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include <QDialog>

class QComboBox;
class QDateTimeEdit;
class QLabel;
class QSpinBox;

class WebFilterDurationDialog : public QDialog
{
	Q_OBJECT
public:
	explicit WebFilterDurationDialog(QWidget* parent, qint64 maxTtlMs);

	qint64 durationMs() const;

private:
	enum class Choice
	{
		Minutes10,
		Minutes20,
		Minutes30,
		Minutes45,
		Minutes60,
		CustomDuration,
		CustomEndTime
	};

	void updateSummary();
	qint64 selectedDurationMs() const;

	qint64 m_maxTtlMs = 0;
	QComboBox* m_choice = nullptr;
	QSpinBox* m_customMinutes = nullptr;
	QDateTimeEdit* m_customEnd = nullptr;
	QLabel* m_endsAt = nullptr;
};
