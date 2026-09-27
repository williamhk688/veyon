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
#include "PersistentWebFilterState.h"
#include "WebFilterConfiguration.h"

#ifdef Q_OS_WIN
class WindowsWebFilterIpcServer;
#endif
class QTimer;
class VeyonMasterInterface;
class VeyonServerInterface;
class WebFilterSessionStatusDialog;
class WebFilterSessionWatchdog;
class WebFilterStatusOverlay;

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
		Domains,
		ExtraProxies,
		Mode,
		SessionId,
		DurationMs,
		StartTimeMs,
		ExpiresAtMs,
		Version,
		StopReason
	};
	Q_ENUM(Argument)

	explicit WebFilterFeaturePlugin(QObject* parent = nullptr);
	~WebFilterFeaturePlugin() override;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{QStringLiteral("7c4e2b91-6a18-4d5e-9f3c-1b8e0a47d2c5")};
	}

	QVersionNumber version() const override
	{
		return QVersionNumber(1, 17);
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

	bool handleFeatureMessage(VeyonWorkerInterface& worker, const FeatureMessage& message) override;

	void initializeServer(VeyonServerInterface& server) override;

	bool handleFeatureMessageFromWorker(VeyonServerInterface& server, const FeatureMessage& message) override;

	bool isFeatureActive(VeyonServerInterface& server, Feature::Uid featureUid) const override;

	ConfigurationPage* createConfigurationPage() override;

private:
	enum class FeatureCommand
	{
		ApplyBlacklist,
		ApplyWhitelist,
		Restore,
		ShowStatus,
		HideStatus,
		FailsafeUnlock
	};

	void startServiceHelper();
	Feature::Uid overlayFeatureUid(WebFilterSession::Mode mode) const;
	void stopOverlayWorker(VeyonServerInterface& server, Feature::Uid featureUid);
	void showOverlayWorker(VeyonServerInterface& server, WebFilterSession::Mode mode);
	void hideOverlayWorker(VeyonServerInterface& server);
	void restoreOverlayWorker();
	void syncPersistedSession();
	WebFilterSession sessionFromMessage(const FeatureMessage& message) const;
	void addSessionArguments(FeatureMessage& message, const WebFilterSession& session) const;
	bool promptDurationAndStart(VeyonMasterInterface& master,
								const Feature& feature,
								const ComputerControlInterfaceList& computerControlInterfaces);
	void startTeacherSession(const WebFilterSession& session, VeyonMasterInterface& master);
	void stopTeacherSession(WebFilterSession::StopReason reason);
	void rebuildFeatureList();
	void refreshTeacherUi();
	void showStatusDialog();
	void onTeacherTimerExpired();
	void onTeacherTick();
	qint64 configuredMaxTtlMs() const;
	QStringList configuredBlockedDomains() const;
	QStringList configuredAllowedDomains() const;
	QStringList configuredExtraProxyDomains() const;

	WebFilterConfiguration m_configuration;
	WebFilterSession m_activeSession;
	VeyonMasterInterface* m_master = nullptr;
	WebFilterSessionStatusDialog* m_statusDialog = nullptr;
	WebFilterSessionWatchdog* m_watchdog = nullptr;
	QTimer* m_sessionSyncTimer = nullptr;
	QTimer* m_teacherExpireTimer = nullptr;
	QTimer* m_teacherTickTimer = nullptr;
	const Feature m_webFilterFeature;
	const Feature m_blacklistFeature;
	const Feature m_whitelistFeature;
	const Feature m_restoreFeature;
	const Feature m_viewStatusFeature;
	FeatureList m_features;
	WebFilterStatusOverlay* m_statusOverlay = nullptr;
	VeyonServerInterface* m_server = nullptr;
#ifdef Q_OS_WIN
	WindowsWebFilterIpcServer* m_ipcServer = nullptr;
#endif
};
