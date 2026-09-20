/*
 * TeacherSelfRescue.cpp - gated teacher recovery handbook shown in Master
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

#include <QAbstractButton>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "FailsafePasswordState.h"
#include "FailsafeUnlock.h"
#include "TeacherSelfRescue.h"


QString TeacherSelfRescue::normalizeAnswer(const QString& answer)
{
	auto normalized = answer.trimmed().simplified().toUpper();
	normalized.remove(QLatin1Char(' '));
	normalized.remove(QLatin1Char('.'));
	normalized.remove(QChar(0x3000)); // ideographic space

	const auto stripSuffix = [&normalized](const QString& suffix) {
		if (normalized.endsWith(suffix))
		{
			normalized.chop(suffix.size());
		}
	};

	stripSuffix(QStringLiteral("SIR"));
	stripSuffix(QStringLiteral("ROOM"));
	stripSuffix(QString::fromUtf8("老師"));
	stripSuffix(QString::fromUtf8("室"));
	stripSuffix(QString::fromUtf8("年"));
	return normalized;
}



bool TeacherSelfRescue::answersMatch(const QString& itCoordinatorInitials,
									 const QString& steamFormerRoom,
									 const QString& firstComputerTeacher)
{
	return normalizeAnswer(itCoordinatorInitials) == QLatin1String(ExpectedItCoordinatorInitials) &&
			normalizeAnswer(steamFormerRoom) == QLatin1String(ExpectedSteamFormerRoom) &&
			normalizeAnswer(firstComputerTeacher) == QLatin1String(ExpectedFirstComputerTeacher);
}



void TeacherSelfRescue::run(QWidget* parent)
{
	if (confirmPrivacyWarning(parent) == false)
	{
		return;
	}

	if (promptSecurityQuestions(parent) == false)
	{
		return;
	}

	showHandbook(parent);
}



bool TeacherSelfRescue::confirmPrivacyWarning(QWidget* parent)
{
	QMessageBox box(parent);
	box.setObjectName(QStringLiteral("teacherSelfRescueWarning"));
	box.setIcon(QMessageBox::Warning);
	box.setWindowTitle(QCoreApplication::translate("TeacherSelfRescue",
												   "教師自救手冊 (Teacher Self-Rescue)"));
	box.setText(QCoreApplication::translate("TeacherSelfRescue",
											"This handbook contains unlock steps for locked student PCs."));
	box.setInformativeText(QCoreApplication::translate(
		"TeacherSelfRescue",
		"<p><b>開啟前請確認：</b></p>"
		"<ul>"
		"<li>課室裡<b>沒有同學在場</b></li>"
		"<li>Master <b>沒有正在投屏／演示</b>（有的話請先 Stop Demo）</li>"
		"</ul>"
		"<p>此內容只供教師閱讀，請勿讓學生看見螢幕，亦請勿影印張貼。</p>"
		"<p>Before opening, confirm no students are present and that demo / "
		"screen sharing is not running.</p>"));
	box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
	box.setDefaultButton(QMessageBox::Cancel);
	if (auto* yesButton = box.button(QMessageBox::Yes))
	{
		yesButton->setText(QCoreApplication::translate("TeacherSelfRescue",
													   "沒有學生、沒有投屏，繼續"));
	}
	if (auto* cancelButton = box.button(QMessageBox::Cancel))
	{
		cancelButton->setText(QCoreApplication::translate("TeacherSelfRescue", "取消"));
	}
	box.setWindowModality(Qt::ApplicationModal);
	return box.exec() == QMessageBox::Yes;
}



bool TeacherSelfRescue::promptSecurityQuestions(QWidget* parent)
{
	QDialog dialog(parent);
	dialog.setObjectName(QStringLiteral("teacherSelfRescueQuiz"));
	dialog.setWindowTitle(QCoreApplication::translate("TeacherSelfRescue", "安全問題"));
	dialog.setWindowModality(Qt::ApplicationModal);
	dialog.setMinimumWidth(560);

	auto* layout = new QVBoxLayout(&dialog);
	auto* intro = new QLabel(
		QCoreApplication::translate("TeacherSelfRescue",
									"請回答以下三題（忽略大小寫）。答對後才會顯示自救步驟。"),
		&dialog);
	intro->setWordWrap(true);
	layout->addWidget(intro);

	auto* form = new QFormLayout;
	auto* q1Edit = new QLineEdit(&dialog);
	q1Edit->setObjectName(QStringLiteral("teacherSelfRescueAnswer1"));
	auto* roomEdit = new QLineEdit(&dialog);
	roomEdit->setObjectName(QStringLiteral("teacherSelfRescueAnswer2"));
	auto* teacherEdit = new QLineEdit(&dialog);
	teacherEdit->setObjectName(QStringLiteral("teacherSelfRescueAnswer3"));

	form->addRow(QCoreApplication::translate("TeacherSelfRescue",
											 "Q1: 本校資訊科技組主管老師是 ___ (Name initial)。"), q1Edit);
	form->addRow(QCoreApplication::translate("TeacherSelfRescue",
											 "Q2: STEAM 室的前身為 ___ 室。"), roomEdit);
	form->addRow(QCoreApplication::translate("TeacherSelfRescue",
											 "Q3: 本校第一位電腦科教師是 ___ Sir。"), teacherEdit);
	layout->addLayout(form);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	buttons->button(QDialogButtonBox::Ok)->setText(
		QCoreApplication::translate("TeacherSelfRescue", "提交"));
	buttons->button(QDialogButtonBox::Cancel)->setText(
		QCoreApplication::translate("TeacherSelfRescue", "取消"));
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
		if (answersMatch(q1Edit->text(), roomEdit->text(), teacherEdit->text()))
		{
			dialog.accept();
			return;
		}

		QMessageBox::warning(&dialog,
							 QCoreApplication::translate("TeacherSelfRescue", "安全問題"),
							 QCoreApplication::translate("TeacherSelfRescue",
														 "答案不正確，無法開啟自救手冊。請再試一次。"));
		q1Edit->selectAll();
		q1Edit->setFocus();
	});
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);

	q1Edit->setFocus();
	return dialog.exec() == QDialog::Accepted;
}



void TeacherSelfRescue::showHandbook(QWidget* parent)
{
	QDialog dialog(parent);
	dialog.setObjectName(QStringLiteral("teacherSelfRescueHandbook"));
	dialog.setWindowTitle(QCoreApplication::translate("TeacherSelfRescue",
													  "教師自救手冊 (Teacher Self-Rescue)"));
	dialog.setWindowModality(Qt::ApplicationModal);
	dialog.resize(760, 560);

	auto* layout = new QVBoxLayout(&dialog);
	auto* heading = new QLabel(
		QCoreApplication::translate("TeacherSelfRescue",
									"僅供教師。一般上課請先用 Master 的 Unlock / Stop Demo。"),
		&dialog);
	heading->setWordWrap(true);
	layout->addWidget(heading);

	auto* tabs = new QTabWidget(&dialog);
	tabs->setObjectName(QStringLiteral("teacherSelfRescueSteps"));
	const QStringList titles{
		QCoreApplication::translate("TeacherSelfRescue", "步驟 1　簡單排查"),
		QCoreApplication::translate("TeacherSelfRescue", "步驟 2　熱鍵解鎖"),
		QCoreApplication::translate("TeacherSelfRescue", "步驟 3　安全模式")
	};
	for (int step = 1; step <= 3; ++step)
	{
		auto* browser = new QTextBrowser(tabs);
		browser->setOpenExternalLinks(false);
		browser->setHtml(handbookHtml(step));
		tabs->addTab(browser, titles.at(step - 1));
	}
	layout->addWidget(tabs, 1);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
	buttons->button(QDialogButtonBox::Close)->setText(
		QCoreApplication::translate("TeacherSelfRescue", "關閉"));
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	layout->addWidget(buttons);

	dialog.exec();
}



QString TeacherSelfRescue::handbookHtml(int step)
{
	const auto hotkey = QString::fromLatin1(FailsafeUnlock::HotkeySequence);
	const auto password = FailsafePasswordState::handbookPassword().toHtmlEscaped();
	const auto hotkeyHtml = hotkey.toHtmlEscaped();

	switch (step)
	{
	case 1:
		return QCoreApplication::translate(
			"TeacherSelfRescue",
			"<h3>步驟 1 — 先做簡單排查</h3>"
			"<p>學生電腦變磚時，多數情況仍可用老師電腦解開，不必進安全模式。</p>"
			"<ol>"
			"<li><b>用 Veyon Master 解鎖：</b>選取該學生電腦，按 Unlock（解鎖）。"
			"若正在演示，先按 Stop Demo（停止演示）。這是正規做法。</li>"
			"<li><b>網絡斷了？</b>拔網線<b>不能</b>自行解鎖。鎖定旗標記在學生本機。"
			"請把網線插回（或接上 Wi-Fi），等 Master 再連上後，再按一次 Unlock / Stop Demo。</li>"
			"<li><b>老師 Master 能連上、學生畫面仍鎖：</b>再按一次 Unlock。"
			"關機後仍鎖是設計行為，要等 Master 解鎖才會清掉登錄檔旗標。</li>"
			"<li>只有 Master 連不到、鍵盤滑鼠完全沒反應（連 Ctrl+Alt+Delete 也沒有）時，"
			"才用步驟 2 的熱鍵，或步驟 3 的安全模式。</li>"
			"</ol>");
	case 2:
		return QCoreApplication::translate(
			"TeacherSelfRescue",
			"<h3>步驟 2 — 在被鎖的電腦上用熱鍵解鎖</h3>"
			"<p>適用：學生機已鎖、鍵盤被 Interception 攔截，但你人在該機前面。</p>"
			"<ol>"
			"<li>在<b>被鎖的那一台</b>按下：<b>%1</b></li>"
			"<li>會出現密碼框。請輸入熱鍵解鎖密碼。</li>"
			"<li><b>最新解鎖密碼：</b><code style=\"font-size:16px\">%2</code></li>"
			"</ol>"
			"<p>此密碼只在通過安全問題後顯示。從未用「修改解鎖密碼」成功寫入學生機時，"
			"此處為預設值。若上次只改到部分電腦，失敗的那幾台可能仍是舊密碼。</p>"
			"<p>密碼框開啟時只能輸入英數字、符號、Enter、Backspace；"
			"Ctrl、Alt、Win、Esc 仍會被攔截，無法開工作管理員。</p>"
			"<p>成功後會清掉本機的鎖定／演示旗標，鍵盤滑鼠應立即恢復。"
			"回到老師 Master，若該學生仍顯示鎖定，再按一次 Unlock 對齊狀態。</p>"
			"<p>熱鍵無效（驅動未載入、或該機沒裝 Interception）時，請改用步驟 3。</p>")
			.arg(hotkeyHtml, password);
	case 3:
		return QCoreApplication::translate(
			"TeacherSelfRescue",
			"<h3>步驟 3 — Windows 安全模式／修復環境</h3>"
			"<p>適用：熱鍵無效、鍵盤完全沒反應。目標有兩件事："
			"先讓鍵盤能用，再刪掉本機記住的鎖定旗標。"
			"只停 Veyon 服務但不刪登錄檔的話，下次正常開機還會再鎖。</p>"
			"<p><b>3.1 鍵盤已死：用電源鍵進修復環境</b></p>"
			"<ol>"
			"<li>長按電源鍵 5–10 秒直到完全熄滅（筆電按機身電源，不是蓋上螢幕）。</li>"
			"<li>再開機，看到 Windows 標誌或旋轉圓點時，立刻再長按電源強迫關機。</li>"
			"<li>重複 2–3 次。之後 Windows 應進入「自動修復」。等到「疑難排解」／「進階選項」，不要再強制關機。</li>"
			"<li>選「疑難排解」→「進階選項」。修復環境通常不載入 Interception，鍵盤可用。"
			"若仍無反應，改插有線 USB 鍵盤到主機機殼 USB 孔。</li>"
			"</ol>"
			"<p><b>3.2 進入安全模式</b></p>"
			"<ol>"
			"<li>進階選項 →「啟動設定」→「重新啟動」。</li>"
			"<li>出現號碼後按 <b>4</b> 或 <b>F4</b>＝安全模式。需要網域登入才按 5 / F5。</li>"
			"<li>用本機系統管理員或學校教師／IT 管理員帳號登入（學生帳號通常刪不了登錄檔）。</li>"
			"</ol>"
			"<p><b>3.3 停止服務並刪除鎖定旗標（最關鍵）</b></p>"
			"<p>系統管理員命令提示字元可複製：</p>"
			"<pre>sc stop VeyonService\n"
			"reg delete \"HKLM\\SOFTWARE\\Veyon Solutions\\VeyonScreenLock\" /f\n"
			"reg delete \"HKLM\\SOFTWARE\\Veyon Solutions\\VeyonDemo\" /f</pre>"
			"<p>圖形介面：services.msc 停止「Veyon Service」；regedit 刪除 "
			"<code>HKLM\\SOFTWARE\\Veyon Solutions</code> 底下的 "
			"<b>VeyonScreenLock</b> 與 <b>VeyonDemo</b> 資料夾。"
			"不要刪 <code>Veyon</code> 資料夾（教室設定在裡面）。走 64 位元路徑，不要進 Wow6432Node。</p>"
			"<p>然後開始功能表 → 重新啟動。正常開機後鍵盤應恢復。到 Master 再按一次 Unlock 對齊狀態。</p>"
			"<p><b>進不了安全模式時：</b>修復環境 → 命令提示字元。先用 "
			"<code>dir C:\\Windows\\System32\\config\\SOFTWARE</code> 確認系統碟"
			"（有時是 D:），然後：</p>"
			"<pre>reg load HKLM\\VeyonRec C:\\Windows\\System32\\config\\SOFTWARE\n"
			"reg delete \"HKLM\\VeyonRec\\Veyon Solutions\\VeyonScreenLock\" /f\n"
			"reg delete \"HKLM\\VeyonRec\\Veyon Solutions\\VeyonDemo\" /f\n"
			"reg unload HKLM\\VeyonRec</pre>"
			"<p><code>unload</code> 一定要做。然後 <code>exit</code>，選繼續開機。</p>"
			"<p><b>請不要：</b>把這份說明貼在課室、教學生進安全模式、格式化硬碟、"
			"刪除整個 Veyon 機碼、或在老師電腦清登錄檔來救學生"
			"（旗標只存在被鎖的那一台）。不要卸載 Interception，否則以後鎖定擋不住 Ctrl+Alt+Delete。</p>");
	default:
		return {};
	}
}
