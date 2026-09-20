/*
 * FailsafePasswordFeaturePlugin.cpp - Master/Server feature for failsafe password distribution
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

#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "ComputerControlInterface.h"
#include "FailsafePasswordFeaturePlugin.h"
#include "FailsafePasswordState.h"
#include "Logger.h"
#include "TeacherSelfRescue.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VncConnection.h"


FailsafePasswordFeaturePlugin::FailsafePasswordFeaturePlugin(QObject* parent) :
	QObject(parent),
	m_changePasswordFeature(QStringLiteral("ChangeFailsafePassword"),
							Feature::Flag::Action | Feature::Flag::Master | Feature::Flag::Service,
							Feature::Uid(QStringLiteral("5b8d2c91-4e7a-4f03-9c6b-2a1d8e5f7044")),
							Feature::Uid(),
							tr("修改解鎖密碼 (Change Failsafe Password)"), {},
							tr("Send a new failsafe unlock password to the selected computers "
							   "and report which clients stored it successfully."),
							QStringLiteral(":/core/document-edit.png")),
	m_selfRescueFeature(QStringLiteral("TeacherSelfRescue"),
						Feature::Flag::Action | Feature::Flag::Master,
						Feature::Uid(QStringLiteral("8d1f6e2a-4c9b-4a73-b5e0-1c7f9a2d6b48")),
						Feature::Uid(),
						tr("教師自救手冊 (Teacher Self-Rescue)"), {},
						tr("Open the teacher self-rescue handbook after a privacy warning "
						   "and three security questions. Does not send anything to students."),
						QStringLiteral(":/core/help-about.png")),
	m_features({ m_changePasswordFeature, m_selfRescueFeature })
{
}



bool FailsafePasswordFeaturePlugin::controlFeature(Feature::Uid featureUid, Operation operation,
												   const QVariantMap& arguments,
												   const ComputerControlInterfaceList& computerControlInterfaces)
{
	if (operation != Operation::Start || featureUid != m_changePasswordFeature.uid())
	{
		return false;
	}

	const auto password = arguments.value(argToString(Argument::Password)).toString();
	const auto requestId = arguments.value(argToString(Argument::RequestId));

	sendFeatureMessage(FeatureMessage{featureUid, FeatureCommand::SetPassword}
					   .addArgument(Argument::Password, password)
					   .addArgument(Argument::RequestId, requestId),
					   computerControlInterfaces);
	return true;
}



bool FailsafePasswordFeaturePlugin::startFeature(VeyonMasterInterface& master, const Feature& feature,
												 const ComputerControlInterfaceList& computerControlInterfaces)
{
	if (feature.uid() == m_selfRescueFeature.uid())
	{
		TeacherSelfRescue::run(master.mainWindow());
		return true;
	}

	if (feature.uid() != m_changePasswordFeature.uid())
	{
		return false;
	}

	if (computerControlInterfaces.isEmpty())
	{
		QMessageBox::warning(master.mainWindow(),
							 feature.displayName(),
							 tr("Please select at least one computer."));
		return true;
	}

	bool ok = false;
	const auto password = QInputDialog::getText(master.mainWindow(),
												feature.displayName(),
												tr("Enter the new failsafe unlock password:"),
												QLineEdit::Password, {}, &ok);
	if (ok == false || password.isEmpty())
	{
		return true;
	}

	const auto requestId = QUuid::createUuid();
	PendingRequest request;
	request.interfaces = computerControlInterfaces;

	for (const auto& controlInterface : computerControlInterfaces)
	{
		if (controlInterface->state() != ComputerControlInterface::State::Connected)
		{
			request.failed.insert(controlInterface.data());
		}
	}

	m_pendingRequests.insert(requestId, request);

	controlFeature(m_changePasswordFeature.uid(), Operation::Start,
				   {
					   { argToString(Argument::Password), password },
					   { argToString(Argument::RequestId), requestId.toString(QUuid::WithoutBraces) }
				   },
				   computerControlInterfaces);

	waitForAcknowledgements(master.mainWindow(), requestId);

	const auto finishedRequest = m_pendingRequests.take(requestId);
	showResults(master.mainWindow(), finishedRequest);
	return true;
}



bool FailsafePasswordFeaturePlugin::handleFeatureMessage(ComputerControlInterface::Pointer computerControlInterface,
														 const FeatureMessage& message)
{
	if (message.featureUid() != m_changePasswordFeature.uid() ||
		message.command<FeatureCommand>() != FeatureCommand::SetPasswordAck)
	{
		return false;
	}

	const QUuid requestId{message.argument(Argument::RequestId).toString()};
	auto it = m_pendingRequests.find(requestId);
	if (it == m_pendingRequests.end())
	{
		return true;
	}

	if (message.argument(Argument::Success).toBool())
	{
		it->succeeded.insert(computerControlInterface.data());
		it->failed.remove(computerControlInterface.data());
	}
	else
	{
		it->failed.insert(computerControlInterface.data());
		it->succeeded.remove(computerControlInterface.data());
	}

	Q_EMIT acknowledgementReceived(requestId);
	return true;
}



bool FailsafePasswordFeaturePlugin::handleFeatureMessage(VeyonServerInterface& server,
														 const MessageContext& messageContext,
														 const FeatureMessage& message)
{
	if (message.featureUid() != m_changePasswordFeature.uid() ||
		message.command<FeatureCommand>() != FeatureCommand::SetPassword)
	{
		return false;
	}

	const auto password = message.argument(Argument::Password).toString();
	const auto success = FailsafePasswordState::setPassword(password);

	if (success == false)
	{
		vWarning() << "failed to store failsafe unlock password";
	}

	return server.sendFeatureMessageReply(messageContext,
										  FeatureMessage{m_changePasswordFeature.uid(), FeatureCommand::SetPasswordAck}
										  .addArgument(Argument::RequestId, message.argument(Argument::RequestId))
										  .addArgument(Argument::Success, success));
}



QString FailsafePasswordFeaturePlugin::computerLabel(const ComputerControlInterface::Pointer& controlInterface) const
{
	auto label = controlInterface->computerName();
	if (label.isEmpty())
	{
		label = controlInterface->computer().hostName();
	}
	if (label.isEmpty())
	{
		label = controlInterface->computer().displayName();
	}
	return label;
}



void FailsafePasswordFeaturePlugin::waitForAcknowledgements(QWidget* parent, const QUuid& requestId)
{
	auto it = m_pendingRequests.find(requestId);
	if (it == m_pendingRequests.end())
	{
		return;
	}

	auto remainingSeconds = AcknowledgementTimeoutSeconds;

	QProgressDialog progress(parent);
	progress.setWindowTitle(m_changePasswordFeature.displayName());
	progress.setLabelText(tr("Waiting for clients to store the new password… %1 s remaining").arg(remainingSeconds));
	progress.setCancelButtonText(tr("Stop waiting"));
	progress.setRange(0, AcknowledgementTimeoutSeconds);
	progress.setValue(0);
	progress.setWindowModality(Qt::ApplicationModal);
	progress.show();

	QEventLoop loop;
	QTimer secondTimer;
	secondTimer.setInterval(1000);

	const auto allResolved = [this, requestId]() {
		const auto current = m_pendingRequests.value(requestId);
		return current.succeeded.count() + current.failed.count() >= current.interfaces.count();
	};

	const auto allSucceeded = [this, requestId]() {
		const auto current = m_pendingRequests.value(requestId);
		return current.succeeded.count() == current.interfaces.count();
	};

	connect(&secondTimer, &QTimer::timeout, &loop, [&]() {
		remainingSeconds--;
		progress.setValue(AcknowledgementTimeoutSeconds - remainingSeconds);
		progress.setLabelText(tr("Waiting for clients to store the new password… %1 s remaining")
							  .arg(qMax(remainingSeconds, 0)));
		if (remainingSeconds <= 0 || allResolved())
		{
			loop.quit();
		}
	});

	connect(this, &FailsafePasswordFeaturePlugin::acknowledgementReceived, &loop, [&](QUuid id) {
		if (id == requestId && (allSucceeded() || allResolved()))
		{
			loop.quit();
		}
	});

	connect(&progress, &QProgressDialog::canceled, &loop, &QEventLoop::quit);

	if (allResolved() || allSucceeded())
	{
		return;
	}

	secondTimer.start();
	loop.exec();
}



void FailsafePasswordFeaturePlugin::showResults(QWidget* parent, const PendingRequest& request) const
{
	QStringList succeeded;
	QStringList failed;

	for (const auto& controlInterface : request.interfaces)
	{
		const auto label = computerLabel(controlInterface);
		if (request.succeeded.contains(controlInterface.data()))
		{
			succeeded.append(label);
		}
		else
		{
			failed.append(label);
		}
	}

	succeeded.sort();
	failed.sort();

	QDialog dialog(parent);
	dialog.setWindowTitle(m_changePasswordFeature.displayName());
	dialog.resize(560, 420);

	auto* layout = new QVBoxLayout(&dialog);
	layout->addWidget(new QLabel(tr("Updated %1 of %2 computer(s).")
								 .arg(succeeded.count())
								 .arg(request.interfaces.count()), &dialog));

	auto* succeededBox = new QGroupBox(tr("Successfully updated"), &dialog);
	auto* succeededList = new QListWidget(succeededBox);
	succeededList->addItems(succeeded);
	auto* succeededLayout = new QVBoxLayout(succeededBox);
	succeededLayout->addWidget(succeededList);
	layout->addWidget(succeededBox);

	auto* failedBox = new QGroupBox(tr("Failed or did not respond"), &dialog);
	auto* failedList = new QListWidget(failedBox);
	failedList->addItems(failed);
	auto* failedLayout = new QVBoxLayout(failedBox);
	failedLayout->addWidget(failedList);
	layout->addWidget(failedBox);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	layout->addWidget(buttons);

	dialog.exec();
}
