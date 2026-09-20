/*
 * TeacherSelfRescue.h - gated teacher recovery handbook shown in Master
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include <QString>
#include <QStringList>

class QWidget;

/*!
 * Master-only teacher self-rescue handbook. A privacy warning and three
 * school-specific questions must succeed (case-insensitive) before the
 * unlock / Safe Mode steps are shown. Nothing is sent to student PCs.
 */
class TeacherSelfRescue
{
public:
	static QString normalizeAnswer(const QString& answer);
	static bool answersMatch(const QString& itCoordinatorInitials,
							 const QString& steamFormerRoom,
							 const QString& firstComputerTeacher);

	/*! Warning → quiz (retry until correct or cancelled) → handbook. */
	static void run(QWidget* parent);

	/*!
	 * Same three security questions used by the handbook. Retry until
	 * correct or cancelled. Optional copy overrides the handbook wording
	 * so Change Failsafe Password can reuse the quiz as its gate.
	 */
	static bool promptSecurityQuestions(QWidget* parent,
										const QString& introText = {},
										const QString& mismatchText = {});

	static constexpr auto ExpectedItCoordinatorInitials = "LCC";
	static constexpr auto ExpectedSteamFormerRoom = "DAT";
	static constexpr auto ExpectedFirstComputerTeacher = "WAN";

private:
	static bool confirmPrivacyWarning(QWidget* parent);
	static void showHandbook(QWidget* parent);
	static QString handbookHtml(int step);
};
