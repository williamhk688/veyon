/*
 * WebFilterFeaturePlugin.cpp - Master dropdown and student-side apply
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <algorithm>
#include <limits>

#include <QCoreApplication>
#include <QDateTime>
#include <QMessageBox>
#include <QTimer>

#include "ComputerControlInterface.h"
#include "FailsafeUnlock.h"
#include "FeatureWorkerManager.h"
#include "PersistentWebFilterState.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"
#include "WebFilterConfigurationPage.h"
#include "WebFilterDurationDialog.h"
#include "WebFilterEngine.h"
#include "WebFilterFeaturePlugin.h"
#include "WebFilterLists.h"
#include "WebFilterSessionPolicy.h"
#include "WebFilterSessionStatusDialog.h"
#include "WebFilterSessionWatchdog.h"
#include "WebFilterStatusOverlay.h"

#ifdef Q_OS_WIN
#include "WebFilterEngine.h"
#include "WindowsWebFilterIpc.h"
#endif


WebFilterFeaturePlugin::WebFilterFeaturePlugin(QObject* parent) :
	QObject(parent),
	m_configuration(&VeyonCore::config()),
	m_webFilterFeature(QStringLiteral("WebFilter"),
					   Feature::Flag::Action | Feature::Flag::AllComponents,
					   Feature::Uid(QStringLiteral("d4e3f6c5-1b7a-4e4c-af9d-5a8102b4e763")),
					   Feature::Uid(),
					   tr("網絡管制 (Web filter)"), {},
					   tr("Choose blacklist, classroom-only whitelist, or restore web access."),
					   QStringLiteral(":/webfilter/web-filter.png")),
	m_blacklistFeature(QStringLiteral("WebFilterBlacklist"),
					   Feature::Flag::Action | Feature::Flag::AllComponents,
					   Feature::Uid(QStringLiteral("a1f0c3d2-8e47-4b19-9c6a-2d5e7f81b430")),
					   m_webFilterFeature.uid(),
					   tr("封鎖黑名單網站 (Block blacklist sites)"), {},
					   tr("Block the built-in proxy list, extra school proxies, and the blacklist."),
					   QStringLiteral(":/webfilter/web-filter-block.png")),
	m_whitelistFeature(QStringLiteral("WebFilterWhitelist"),
					   Feature::Flag::Action | Feature::Flag::AllComponents,
					   Feature::Uid(QStringLiteral("b2e1d4c3-9f58-4c2a-8d7b-3e6f8092c541")),
					   m_webFilterFeature.uid(),
					   tr("只允許白名單網站 (Allow whitelist sites only)"), {},
					   tr("Allow only the configured whitelist websites. Veyon stays allowed. "
						  "This does not stay active after the student computer restarts."),
					   QStringLiteral(":/webfilter/web-filter-allow.png")),
	m_restoreFeature(QStringLiteral("WebFilterRestore"),
					 Feature::Flag::Action | Feature::Flag::AllComponents,
					 Feature::Uid(QStringLiteral("c3d2e5b4-0a69-4d3b-9e8c-4f7091a3d652")),
					 m_webFilterFeature.uid(),
					 tr("恢復網絡 (Restore web)"), {},
					 tr("Remove the classroom web filter from the selected computers."),
					 QStringLiteral(":/webfilter/web-filter-restore.png")),
	m_viewStatusFeature(QStringLiteral("WebFilterViewStatus"),
						Feature::Flag::Action | Feature::Flag::Master,
						Feature::Uid(QStringLiteral("e5a4b7c6-3d9c-4e6e-91bf-7c0324d6a985")),
						m_webFilterFeature.uid(),
						tr("查看剩餘時間 (View remaining time)"), {},
						tr("Open the current web-filter session remaining time. "
						   "This item is only shown while a session is active."),
						QStringLiteral(":/webfilter/web-filter.png"))
{
	rebuildFeatureList();
	if (VeyonCore::component() == VeyonCore::Component::Service)
	{
		// Only start the IPC thread here. Reconcile/PAC cleanup runs inside
		// that thread so VeyonCore construction and veyon-server spawn are
		// not blocked (and no window is created in this process).
		startServiceHelper();
		m_watchdog = new WebFilterSessionWatchdog(this);
		m_watchdog->start();
	}
}



WebFilterFeaturePlugin::~WebFilterFeaturePlugin()
{
	delete m_statusOverlay;
}



void WebFilterFeaturePlugin::upgrade(const QVersionNumber& oldVersion)
{
	if (oldVersion < QVersionNumber(1, 2))
	{
		if (m_configuration.blockedWebsites().isEmpty())
		{
			m_configuration.setBlockedWebsites(WebFilterLists::toJson(WebFilterLists::defaultBlockedDomains()));
		}
		if (m_configuration.allowedWebsites().isEmpty())
		{
			m_configuration.setAllowedWebsites(WebFilterLists::toJson(WebFilterLists::defaultAllowedDomains()));
		}
	}
}



QStringList WebFilterFeaturePlugin::configuredBlockedDomains() const
{
	return WebFilterLists::fromJson(m_configuration.blockedWebsites());
}



QStringList WebFilterFeaturePlugin::configuredAllowedDomains() const
{
	return WebFilterLists::effectiveAllowlist(WebFilterLists::fromJson(m_configuration.allowedWebsites()),
											  configuredExtraProxyDomains());
}



QStringList WebFilterFeaturePlugin::configuredExtraProxyDomains() const
{
	return WebFilterLists::fromJson(m_configuration.extraProxyWebsites());
}



void WebFilterFeaturePlugin::startServiceHelper()
{
#ifdef Q_OS_WIN
	WebFilterEngine::ensureClassroomFirewall();
	if (m_ipcServer == nullptr)
	{
		m_ipcServer = new WindowsWebFilterIpcServer(this);
		m_ipcServer->start();
	}
#else
	return;
#endif
}



qint64 WebFilterFeaturePlugin::configuredMaxTtlMs() const
{
	const auto minutes = m_configuration.temporaryWebFilterMaxTtlMinutes();
	if (minutes <= 0)
	{
		return WebFilterSessionPolicy::DefaultMaxTtlMs;
	}
	return qint64(minutes) * 60 * 1000;
}



WebFilterSession WebFilterFeaturePlugin::sessionFromMessage(const FeatureMessage& message) const
{
	WebFilterSession session;
	session.sessionId = QUuid::fromString(message.argument(Argument::SessionId).toString());
	session.durationMs = message.argument(Argument::DurationMs).toLongLong();
	session.startTimeMs = message.argument(Argument::StartTimeMs).toLongLong();
	session.expiresAtMs = message.argument(Argument::ExpiresAtMs).toLongLong();
	session.version = message.argument(Argument::Version).toInt();
	session.hardExpiresAtMs = session.startTimeMs + configuredMaxTtlMs();
	session.domains = message.argument(Argument::Domains).toStringList();
	return session;
}



void WebFilterFeaturePlugin::addSessionArguments(FeatureMessage& message, const WebFilterSession& session) const
{
	message.addArgument(Argument::SessionId, session.sessionId.toString(QUuid::WithoutBraces));
	message.addArgument(Argument::DurationMs, session.durationMs);
	message.addArgument(Argument::StartTimeMs, session.startTimeMs);
	message.addArgument(Argument::ExpiresAtMs, session.expiresAtMs);
	message.addArgument(Argument::Version, session.version);
}



void WebFilterFeaturePlugin::startTeacherSession(const WebFilterSession& session, VeyonMasterInterface& master)
{
	m_master = &master;
	m_activeSession = session;
	if (m_teacherExpireTimer == nullptr)
	{
		m_teacherExpireTimer = new QTimer(this);
		m_teacherExpireTimer->setSingleShot(true);
		connect(m_teacherExpireTimer, &QTimer::timeout, this, &WebFilterFeaturePlugin::onTeacherTimerExpired);
	}
	if (m_teacherTickTimer == nullptr)
	{
		m_teacherTickTimer = new QTimer(this);
		connect(m_teacherTickTimer, &QTimer::timeout, this, &WebFilterFeaturePlugin::onTeacherTick);
	}
	m_teacherExpireTimer->start(int(std::min<qint64>(session.durationMs, std::numeric_limits<int>::max())));
	m_teacherTickTimer->start(int(WebFilterSessionPolicy::WatchdogIntervalMs));
	refreshTeacherUi();
}



void WebFilterFeaturePlugin::rebuildFeatureList()
{
	m_features = { m_webFilterFeature, m_blacklistFeature, m_whitelistFeature, m_restoreFeature };
	if (m_activeSession.isActive())
	{
		m_features.append(m_viewStatusFeature);
	}
}



void WebFilterFeaturePlugin::refreshTeacherUi()
{
	rebuildFeatureList();
	if (m_master)
	{
		m_master->reloadSubFeatures();
	}
}



void WebFilterFeaturePlugin::showStatusDialog()
{
	if (m_master == nullptr || m_activeSession.isActive() == false)
	{
		return;
	}

	if (m_statusDialog == nullptr)
	{
		m_statusDialog = new WebFilterSessionStatusDialog(m_master->mainWindow());
		connect(m_statusDialog, &WebFilterSessionStatusDialog::restoreRequested, this, [this]() {
			stopTeacherSession(WebFilterSession::StopReason::TeacherManual);
		});
	}
	m_statusDialog->setSession(m_activeSession);
	m_statusDialog->show();
	m_statusDialog->raise();
	m_statusDialog->activateWindow();
}



void WebFilterFeaturePlugin::stopTeacherSession(WebFilterSession::StopReason reason)
{
	const auto session = m_activeSession;
	if (session.isActive() == false && reason != WebFilterSession::StopReason::TeacherManual)
	{
		return;
	}

	if (m_teacherExpireTimer)
	{
		m_teacherExpireTimer->stop();
	}
	if (m_teacherTickTimer)
	{
		m_teacherTickTimer->stop();
	}
	m_activeSession = {};
	if (m_statusDialog)
	{
		m_statusDialog->hide();
	}
	refreshTeacherUi();

	if (m_master == nullptr)
	{
		return;
	}

	auto targets = m_master->allComputerControlInterfaces();
	if (targets.isEmpty())
	{
		targets = m_master->filteredComputerControlInterfaces();
	}
	FeatureMessage message{m_restoreFeature.uid(), FeatureCommand::Restore};
	addSessionArguments(message, session);
	message.addArgument(Argument::StopReason, int(reason));
	sendFeatureMessage(message, targets);
}



void WebFilterFeaturePlugin::onTeacherTimerExpired()
{
	if (WebFilterSessionPolicy::shouldFireTimer(m_activeSession.sessionId, m_activeSession.sessionId) == false)
	{
		return;
	}
	stopTeacherSession(WebFilterSession::StopReason::TeacherTimerExpired);
}



void WebFilterFeaturePlugin::onTeacherTick()
{
	if (m_activeSession.isActive() == false)
	{
		if (m_statusDialog)
		{
			m_statusDialog->hide();
		}
		return;
	}

	if (m_statusDialog && m_statusDialog->isVisible())
	{
		m_statusDialog->setSession(m_activeSession);
	}

	if (m_master == nullptr)
	{
		return;
	}

	const auto childUid = overlayFeatureUid(m_activeSession.mode);
	for (const auto& controlInterface : m_master->allComputerControlInterfaces())
	{
		if (controlInterface == nullptr ||
			controlInterface->state() != ComputerControlInterface::State::Connected)
		{
			continue;
		}
		if (controlInterface->activeFeatures().contains(childUid))
		{
			continue;
		}
		FeatureMessage message{childUid,
							   m_activeSession.mode == WebFilterSession::Mode::Whitelist ?
								   FeatureCommand::ApplyWhitelist : FeatureCommand::ApplyBlacklist};
		message.addArgument(Argument::Domains, m_activeSession.mode == WebFilterSession::Mode::Whitelist ?
								configuredAllowedDomains() : configuredBlockedDomains());
		message.addArgument(Argument::ExtraProxies, configuredExtraProxyDomains());
		addSessionArguments(message, m_activeSession);
		sendFeatureMessage(message, {controlInterface});
	}
}



bool WebFilterFeaturePlugin::promptDurationAndStart(VeyonMasterInterface& master,
													const Feature& feature,
													const ComputerControlInterfaceList& computerControlInterfaces)
{
	WebFilterDurationDialog dialog(master.mainWindow(), configuredMaxTtlMs());
	if (dialog.exec() != QDialog::Accepted)
	{
		return true;
	}

	const auto mode = feature.uid() == m_whitelistFeature.uid() ?
						  WebFilterSession::Mode::Whitelist : WebFilterSession::Mode::Blacklist;
	auto session = WebFilterSessionPolicy::create(mode, dialog.durationMs(),
												  QDateTime::currentMSecsSinceEpoch(),
												  configuredMaxTtlMs(),
												  mode == WebFilterSession::Mode::Whitelist ?
													  configuredAllowedDomains() : configuredBlockedDomains());
	startTeacherSession(session, master);

	QVariantMap arguments;
	arguments.insert(argToString(Argument::SessionId), session.sessionId.toString(QUuid::WithoutBraces));
	arguments.insert(argToString(Argument::DurationMs), session.durationMs);
	arguments.insert(argToString(Argument::StartTimeMs), session.startTimeMs);
	arguments.insert(argToString(Argument::ExpiresAtMs), session.expiresAtMs);
	arguments.insert(argToString(Argument::Version), session.version);
	return controlFeature(feature.uid(), Operation::Start, arguments, computerControlInterfaces);
}



Feature::Uid WebFilterFeaturePlugin::overlayFeatureUid(WebFilterSession::Mode mode) const
{
	switch (mode)
	{
	case PersistentWebFilterState::Mode::Blacklist:
		return m_blacklistFeature.uid();
	case PersistentWebFilterState::Mode::Whitelist:
		return m_whitelistFeature.uid();
	case PersistentWebFilterState::Mode::Off:
		break;
	}

	return {};
}



void WebFilterFeaturePlugin::stopOverlayWorker(VeyonServerInterface& server, Feature::Uid featureUid)
{
	if (featureUid.isNull())
	{
		return;
	}

	auto& manager = server.featureWorkerManager();
	if (manager.isWorkerRunning(featureUid))
	{
		manager.sendMessageToManagedSystemWorker(
					FeatureMessage{featureUid, FeatureCommand::HideStatus});
	}
	manager.stopWorker(featureUid);
}



void WebFilterFeaturePlugin::showOverlayWorker(VeyonServerInterface& server,
											  PersistentWebFilterState::Mode mode)
{
	if (mode == PersistentWebFilterState::Mode::Off)
	{
		hideOverlayWorker(server);
		return;
	}

	const auto uid = overlayFeatureUid(mode);
	for (const auto& other : { m_webFilterFeature.uid(),
							   m_blacklistFeature.uid(),
							   m_whitelistFeature.uid() })
	{
		if (other != uid)
		{
			stopOverlayWorker(server, other);
		}
	}

	const auto session = PersistentWebFilterState::session();
	const auto remaining = WebFilterSessionPolicy::remainingMs(
				session.durationMs,
				QDateTime::currentMSecsSinceEpoch() - session.startTimeMs,
				configuredMaxTtlMs());
	server.featureWorkerManager().sendMessageToManagedSystemWorker(
				FeatureMessage{uid, FeatureCommand::ShowStatus}
				.addArgument(Argument::Mode, int(mode))
				.addArgument(Argument::DurationMs, remaining)
				.addArgument(Argument::ExpiresAtMs, session.expiresAtMs));
}



void WebFilterFeaturePlugin::hideOverlayWorker(VeyonServerInterface& server)
{
	stopOverlayWorker(server, m_webFilterFeature.uid());
	stopOverlayWorker(server, m_blacklistFeature.uid());
	stopOverlayWorker(server, m_whitelistFeature.uid());
}



void WebFilterFeaturePlugin::restoreOverlayWorker()
{
	if (m_server == nullptr)
	{
		return;
	}

	if (PersistentWebFilterState::mode() == PersistentWebFilterState::Mode::Off)
	{
		return;
	}

	showOverlayWorker(*m_server, PersistentWebFilterState::mode());
	const auto uid = overlayFeatureUid(PersistentWebFilterState::mode());
	if (uid.isNull() == false &&
		m_server->featureWorkerManager().isWorkerRunning(uid) == false)
	{
		QTimer::singleShot(2000, this, &WebFilterFeaturePlugin::restoreOverlayWorker);
	}
}



bool WebFilterFeaturePlugin::controlFeature(Feature::Uid featureUid, Operation operation,
											const QVariantMap& arguments,
											const ComputerControlInterfaceList& computerControlInterfaces)
{
	if (operation != Operation::Start || hasFeature(featureUid) == false)
	{
		return false;
	}

	if (featureUid == m_webFilterFeature.uid() ||
		featureUid == m_viewStatusFeature.uid())
	{
		return true;
	}

	if (featureUid == m_blacklistFeature.uid())
	{
		auto domains = arguments.value(argToString(Argument::Domains)).toStringList();
		if (domains.isEmpty())
		{
			domains = configuredBlockedDomains();
		}
		auto extra = arguments.value(argToString(Argument::ExtraProxies)).toStringList();
		if (extra.isEmpty())
		{
			extra = configuredExtraProxyDomains();
		}
		FeatureMessage message{featureUid, FeatureCommand::ApplyBlacklist};
		message.addArgument(Argument::Domains, domains);
		message.addArgument(Argument::ExtraProxies, extra);
		if (m_activeSession.isActive())
		{
			addSessionArguments(message, m_activeSession);
		}
		sendFeatureMessage(message, computerControlInterfaces);
		return true;
	}

	if (featureUid == m_whitelistFeature.uid())
	{
		auto domains = arguments.value(argToString(Argument::Domains)).toStringList();
		if (domains.isEmpty())
		{
			domains = configuredAllowedDomains();
		}
		auto extra = arguments.value(argToString(Argument::ExtraProxies)).toStringList();
		if (extra.isEmpty())
		{
			extra = configuredExtraProxyDomains();
		}
		FeatureMessage message{featureUid, FeatureCommand::ApplyWhitelist};
		message.addArgument(Argument::Domains, domains);
		message.addArgument(Argument::ExtraProxies, extra);
		if (m_activeSession.isActive())
		{
			addSessionArguments(message, m_activeSession);
		}
		sendFeatureMessage(message, computerControlInterfaces);
		return true;
	}

	if (featureUid == m_restoreFeature.uid())
	{
		if (m_master)
		{
			stopTeacherSession(WebFilterSession::StopReason::TeacherManual);
			return true;
		}
		FeatureMessage message{featureUid, FeatureCommand::Restore};
		if (m_activeSession.isActive())
		{
			addSessionArguments(message, m_activeSession);
		}
		sendFeatureMessage(message, computerControlInterfaces);
		return true;
	}

	return false;
}



bool WebFilterFeaturePlugin::startFeature(VeyonMasterInterface& master, const Feature& feature,
										  const ComputerControlInterfaceList& computerControlInterfaces)
{
	if (hasFeature(feature.uid()) == false)
	{
		return false;
	}

	if (feature.uid() == m_webFilterFeature.uid())
	{
		return true;
	}

	if (feature.uid() == m_viewStatusFeature.uid())
	{
		m_master = &master;
		showStatusDialog();
		return true;
	}

	if (feature.uid() == m_restoreFeature.uid())
	{
		m_master = &master;
		stopTeacherSession(WebFilterSession::StopReason::TeacherManual);
		return true;
	}

	if (computerControlInterfaces.isEmpty())
	{
		QMessageBox::information(master.mainWindow(),
								 tr("網絡管制 (Web filter)"),
								 tr("請先選取電腦。"));
		return true;
	}

	if (feature.uid() == m_blacklistFeature.uid())
	{
		if (QMessageBox::question(master.mainWindow(),
								  tr("封鎖黑名單網站 (Block blacklist sites)"),
								  tr("將封鎖內建代理站、學校新增的代理站，以及 Configurator 裡的黑名單網站。\n"
									 "Veyon 通訊不受影響。請接著選擇限制時長。"))
			!= QMessageBox::Yes)
		{
			return true;
		}
		return promptDurationAndStart(master, feature, computerControlInterfaces);
	}
	if (feature.uid() == m_whitelistFeature.uid())
	{
		if (QMessageBox::question(master.mainWindow(),
								  tr("只允許白名單網站 (Allow whitelist sites only)"),
								  tr("學生將只能開啟 Configurator 裡的白名單網站。\n"
									 "Veyon 通訊維持可通。限制到期或按恢復網絡後解除，設定清單會保留。"))
			!= QMessageBox::Yes)
		{
			return true;
		}
		return promptDurationAndStart(master, feature, computerControlInterfaces);
	}

	return controlFeature(feature.uid(), Operation::Start, {}, computerControlInterfaces);
}



bool WebFilterFeaturePlugin::handleFeatureMessage(VeyonServerInterface& server,
												  const MessageContext& messageContext,
												  const FeatureMessage& message)
{
	Q_UNUSED(messageContext)

	if (hasFeature(message.featureUid()) == false)
	{
		return false;
	}

#ifdef Q_OS_WIN
	const auto domains = message.argument(Argument::Domains).toStringList();
	const auto extra = message.argument(Argument::ExtraProxies).toStringList();
	auto session = sessionFromMessage(message);
	const auto reason = WebFilterSession::StopReason(
			message.argument(Argument::StopReason).toInt());
	switch (message.command<FeatureCommand>())
	{
	case FeatureCommand::ApplyBlacklist:
		session.mode = WebFilterSession::Mode::Blacklist;
		if (WindowsWebFilterIpcClient::request(WindowsWebFilterIpcClient::Command::Blacklist,
											   domains, extra, session) == false)
		{
			vWarning() << "failed to apply web blacklist";
		}
		showOverlayWorker(server, WebFilterSession::Mode::Blacklist);
		return true;
	case FeatureCommand::ApplyWhitelist:
		session.mode = WebFilterSession::Mode::Whitelist;
		if (WindowsWebFilterIpcClient::request(WindowsWebFilterIpcClient::Command::Whitelist,
											   domains, extra, session) == false)
		{
			vWarning() << "failed to apply web whitelist";
		}
		showOverlayWorker(server, WebFilterSession::Mode::Whitelist);
		return true;
	case FeatureCommand::Restore:
	case FeatureCommand::FailsafeUnlock:
		if (WindowsWebFilterIpcClient::request(WindowsWebFilterIpcClient::Command::Restore,
											   {}, {}, session,
											   message.command<FeatureCommand>() == FeatureCommand::FailsafeUnlock ?
												   WebFilterSession::StopReason::EmergencyUnlock : reason) == false)
		{
			vWarning() << "failed to restore web filter";
		}
		hideOverlayWorker(server);
		return true;
	case FeatureCommand::ShowStatus:
	case FeatureCommand::HideStatus:
		return true;
	}
#else
	Q_UNUSED(server)
	vWarning() << "web filter is only implemented on Windows";
	Q_UNUSED(message)
#endif
	return true;
}



bool WebFilterFeaturePlugin::handleFeatureMessage(VeyonWorkerInterface& worker, const FeatureMessage& message)
{
	if (hasFeature(message.featureUid()) == false)
	{
		return false;
	}

	switch (message.command<FeatureCommand>())
	{
	case FeatureCommand::ShowStatus:
	{
		auto mode = PersistentWebFilterState::Mode(
				message.argument(Argument::Mode).toInt());
		if (mode == PersistentWebFilterState::Mode::Off)
		{
			mode = PersistentWebFilterState::mode();
		}
		if (mode == PersistentWebFilterState::Mode::Off)
		{
			delete m_statusOverlay;
			m_statusOverlay = nullptr;
			return true;
		}
		if (m_statusOverlay == nullptr)
		{
			m_statusOverlay = new WebFilterStatusOverlay;
			connect(m_statusOverlay, &WebFilterStatusOverlay::failsafeUnlocked, this, [&worker, featureUid = message.featureUid()]() {
				worker.sendFeatureMessageReply(FeatureMessage{featureUid, FeatureCommand::FailsafeUnlock});
			});
		}
		m_statusOverlay->setMode(mode);
		m_statusOverlay->setRemainingMs(message.argument(Argument::DurationMs).toLongLong(),
										message.argument(Argument::ExpiresAtMs).toLongLong());
		return true;
	}
	case FeatureCommand::HideStatus:
		delete m_statusOverlay;
		m_statusOverlay = nullptr;
		QTimer::singleShot(0, []() { QCoreApplication::quit(); });
		return true;
	case FeatureCommand::ApplyBlacklist:
	case FeatureCommand::ApplyWhitelist:
	case FeatureCommand::Restore:
	case FeatureCommand::FailsafeUnlock:
		break;
	}

	return false;
}



void WebFilterFeaturePlugin::initializeServer(VeyonServerInterface& server)
{
	m_server = &server;
	restoreOverlayWorker();
}



bool WebFilterFeaturePlugin::handleFeatureMessageFromWorker(VeyonServerInterface& server,
															const FeatureMessage& message)
{
	if (hasFeature(message.featureUid()) == false ||
		message.command<FeatureCommand>() != FeatureCommand::FailsafeUnlock)
	{
		return false;
	}

#ifdef Q_OS_WIN
	const auto session = PersistentWebFilterState::session();
	WindowsWebFilterIpcClient::request(WindowsWebFilterIpcClient::Command::Restore,
									   {}, {}, session,
									   WebFilterSession::StopReason::EmergencyUnlock);
#else
	Q_UNUSED(server)
#endif
	hideOverlayWorker(server);
	return true;
}



bool WebFilterFeaturePlugin::isFeatureActive(VeyonServerInterface& server, Feature::Uid featureUid) const
{
	Q_UNUSED(server)

	const auto mode = PersistentWebFilterState::mode();
	if (featureUid == m_blacklistFeature.uid())
	{
		return mode == PersistentWebFilterState::Mode::Blacklist;
	}
	if (featureUid == m_whitelistFeature.uid())
	{
		return mode == PersistentWebFilterState::Mode::Whitelist;
	}

	return false;
}



ConfigurationPage* WebFilterFeaturePlugin::createConfigurationPage()
{
	return new WebFilterConfigurationPage(m_configuration);
}



IMPLEMENT_CONFIG_PROXY(WebFilterConfiguration)
