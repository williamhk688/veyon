/*
 * FailsafePasswordFeaturePlugin.h - Master/Server feature for failsafe password distribution
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

#include <QMap>
#include <QSet>
#include <QUuid>

#include "Feature.h"
#include "FeatureProviderInterface.h"

class QWidget;

class FailsafePasswordFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.FailsafePassword")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	enum class Argument {
		Password,
		RequestId,
		Success
	};
	Q_ENUM(Argument)

	explicit FailsafePasswordFeaturePlugin(QObject* parent = nullptr);
	~FailsafePasswordFeaturePlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{QStringLiteral("3e9c1a47-8d2f-4b6e-a1c5-7f0e92b4d618")};
	}

	QVersionNumber version() const override
	{
		return QVersionNumber(1, 0);
	}

	QString name() const override
	{
		return QStringLiteral("FailsafePassword");
	}

	QString description() const override
	{
		return tr("Failsafe unlock password distribution and teacher self-rescue handbook");
	}

	QString vendor() const override
	{
		return QStringLiteral("Veyon Community");
	}

	QString copyright() const override
	{
		return QStringLiteral("Tobias Junghans");
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature(Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						const ComputerControlInterfaceList& computerControlInterfaces) override;

	bool startFeature(VeyonMasterInterface& master, const Feature& feature,
					  const ComputerControlInterfaceList& computerControlInterfaces) override;

	bool handleFeatureMessage(ComputerControlInterface::Pointer computerControlInterface,
							  const FeatureMessage& message) override;

	bool handleFeatureMessage(VeyonServerInterface& server,
							  const MessageContext& messageContext,
							  const FeatureMessage& message) override;

private:
	enum class FeatureCommand {
		SetPassword,
		SetPasswordAck
	};

	struct PendingRequest
	{
		ComputerControlInterfaceList interfaces;
		QSet<ComputerControlInterface*> succeeded;
		QSet<ComputerControlInterface*> failed;
	};

	static constexpr int AcknowledgementTimeoutSeconds = 20;

	QString computerLabel(const ComputerControlInterface::Pointer& controlInterface) const;
	void waitForAcknowledgements(QWidget* parent, const QUuid& requestId);
	void showResults(QWidget* parent, const PendingRequest& request) const;

	const Feature m_changePasswordFeature;
	const Feature m_selfRescueFeature;
	const FeatureList m_features;
	QMap<QUuid, PendingRequest> m_pendingRequests;

Q_SIGNALS:
	void acknowledgementReceived(QUuid requestId);
};
