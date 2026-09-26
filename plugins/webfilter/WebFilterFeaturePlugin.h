/*
 * WebFilterFeaturePlugin.h - one-click classroom web blacklist and whitelist
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include "ConfigurationPagePluginInterface.h"
#include "FeatureProviderInterface.h"
#include "WebFilterConfiguration.h"

#ifdef Q_OS_WIN
class WindowsWebFilterIpcServer;
#endif

class WebFilterFeaturePlugin : public QObject, PluginInterface,
		FeatureProviderInterface,
		ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.WebFilter")
	Q_INTERFACES(PluginInterface FeatureProviderInterface ConfigurationPagePluginInterface)
public:
	enum class Argument
	{
		Domains
	};
	Q_ENUM(Argument)

	explicit WebFilterFeaturePlugin(QObject* parent = nullptr);
	~WebFilterFeaturePlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{QStringLiteral("7c4e2b91-6a18-4d5e-9f3c-1b8e0a47d2c5")};
	}

	QVersionNumber version() const override
	{
		return QVersionNumber(1, 2);
	}

	QString name() const override
	{
		return QStringLiteral("WebFilter");
	}

	QString description() const override
	{
		return tr("Block bad websites or allow only classroom websites");
	}

	QString vendor() const override
	{
		return QStringLiteral("Veyon Community");
	}

	QString copyright() const override
	{
		return QStringLiteral("Tobias Junghans");
	}

	void upgrade(const QVersionNumber& oldVersion) override;

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature(Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						const ComputerControlInterfaceList& computerControlInterfaces) override;

	bool startFeature(VeyonMasterInterface& master, const Feature& feature,
					  const ComputerControlInterfaceList& computerControlInterfaces) override;

	bool handleFeatureMessage(VeyonServerInterface& server,
							  const MessageContext& messageContext,
							  const FeatureMessage& message) override;

	ConfigurationPage* createConfigurationPage() override;

private:
	enum class FeatureCommand
	{
		ApplyBlacklist,
		ApplyWhitelist,
		Restore
	};

	void startServiceHelper();
	QStringList configuredBlockedDomains() const;
	QStringList configuredAllowedDomains() const;

	WebFilterConfiguration m_configuration;
	const Feature m_blacklistFeature;
	const Feature m_whitelistFeature;
	const Feature m_restoreFeature;
	const FeatureList m_features;
#ifdef Q_OS_WIN
	WindowsWebFilterIpcServer* m_ipcServer = nullptr;
#endif
};
