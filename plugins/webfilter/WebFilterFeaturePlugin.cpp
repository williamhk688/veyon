/*
 * WebFilterFeaturePlugin.cpp - Master dropdown and student-side apply
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QMessageBox>

#include "ComputerControlInterface.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "WebFilterConfigurationPage.h"
#include "WebFilterFeaturePlugin.h"
#include "WebFilterLists.h"
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
					   tr("封鎖不良網站 (Block websites)"), {},
					   tr("Block the built-in proxy list, extra school proxies, and the bad-website list."),
					   QStringLiteral(":/webfilter/web-filter-block.png")),
	m_whitelistFeature(QStringLiteral("WebFilterWhitelist"),
					   Feature::Flag::Action | Feature::Flag::AllComponents,
					   Feature::Uid(QStringLiteral("b2e1d4c3-9f58-4c2a-8d7b-3e6f8092c541")),
					   m_webFilterFeature.uid(),
					   tr("只准課堂網站 (Classroom sites only)"), {},
					   tr("Allow only the configured classroom websites. Veyon stays allowed. "
						  "This does not stay active after the student computer restarts."),
					   QStringLiteral(":/webfilter/web-filter-allow.png")),
	m_restoreFeature(QStringLiteral("WebFilterRestore"),
					 Feature::Flag::Action | Feature::Flag::AllComponents,
					 Feature::Uid(QStringLiteral("c3d2e5b4-0a69-4d3b-9e8c-4f7091a3d652")),
					 m_webFilterFeature.uid(),
					 tr("恢復網絡 (Restore web)"), {},
					 tr("Remove the classroom web filter from the selected computers."),
					 QStringLiteral(":/webfilter/web-filter-restore.png")),
	m_features({ m_webFilterFeature, m_blacklistFeature, m_whitelistFeature, m_restoreFeature })
{
	if (VeyonCore::component() == VeyonCore::Component::Service)
	{
		startServiceHelper();
	}
	if (VeyonCore::component() == VeyonCore::Component::Server)
	{
		startStatusOverlay();
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
	if (m_ipcServer == nullptr)
	{
		m_ipcServer = new WindowsWebFilterIpcServer(this);
		m_ipcServer->start();
	}
	WebFilterEngine::reconcileOnServiceStart();
#else
	return;
#endif
}



void WebFilterFeaturePlugin::startStatusOverlay()
{
	if (m_statusOverlay == nullptr)
	{
		m_statusOverlay = new WebFilterStatusOverlay;
	}
	refreshStatusOverlay();
}



void WebFilterFeaturePlugin::refreshStatusOverlay()
{
	if (m_statusOverlay)
	{
		m_statusOverlay->syncFromState();
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

	if (featureUid == m_webFilterFeature.uid())
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
		sendFeatureMessage(FeatureMessage{featureUid, FeatureCommand::ApplyBlacklist}
						   .addArgument(Argument::Domains, domains)
						   .addArgument(Argument::ExtraProxies, extra),
						   computerControlInterfaces);
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
		sendFeatureMessage(FeatureMessage{featureUid, FeatureCommand::ApplyWhitelist}
						   .addArgument(Argument::Domains, domains)
						   .addArgument(Argument::ExtraProxies, extra),
						   computerControlInterfaces);
		return true;
	}

	if (featureUid == m_restoreFeature.uid())
	{
		sendFeatureMessage(FeatureMessage{featureUid, FeatureCommand::Restore},
						   computerControlInterfaces);
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
								  tr("封鎖不良網站"),
								  tr("將封鎖內建代理站、學校新增的代理站，以及 Configurator 裡的不良網站。\n"
									 "Veyon 通訊不受影響。是否套用到已選電腦？"))
			!= QMessageBox::Yes)
		{
			return true;
		}
	}
	else if (feature.uid() == m_whitelistFeature.uid())
	{
		if (QMessageBox::question(master.mainWindow(),
								  tr("只准課堂網站"),
								  tr("學生將只能開啟 Configurator 裡的課堂網站。\n"
									 "Veyon 通訊維持可通。重開機後此限制會自動解除。\n"
									 "是否套用到已選電腦？"))
			!= QMessageBox::Yes)
		{
			return true;
		}
	}

	return controlFeature(feature.uid(), Operation::Start, {}, computerControlInterfaces);
}



bool WebFilterFeaturePlugin::handleFeatureMessage(VeyonServerInterface& server,
												  const MessageContext& messageContext,
												  const FeatureMessage& message)
{
	Q_UNUSED(server)
	Q_UNUSED(messageContext)

	if (hasFeature(message.featureUid()) == false)
	{
		return false;
	}

#ifdef Q_OS_WIN
	const auto domains = message.argument(Argument::Domains).toStringList();
	const auto extra = message.argument(Argument::ExtraProxies).toStringList();
	switch (message.command<FeatureCommand>())
	{
	case FeatureCommand::ApplyBlacklist:
		if (WindowsWebFilterIpcClient::request(WindowsWebFilterIpcClient::Command::Blacklist, domains, extra) == false)
		{
			vWarning() << "failed to apply web blacklist";
		}
		refreshStatusOverlay();
		return true;
	case FeatureCommand::ApplyWhitelist:
		if (WindowsWebFilterIpcClient::request(WindowsWebFilterIpcClient::Command::Whitelist, domains, extra) == false)
		{
			vWarning() << "failed to apply web whitelist";
		}
		refreshStatusOverlay();
		return true;
	case FeatureCommand::Restore:
		if (WindowsWebFilterIpcClient::request(WindowsWebFilterIpcClient::Command::Restore) == false)
		{
			vWarning() << "failed to restore web filter";
		}
		refreshStatusOverlay();
		return true;
	}
#else
	vWarning() << "web filter is only implemented on Windows";
	Q_UNUSED(message)
	refreshStatusOverlay();
#endif
	return true;
}



ConfigurationPage* WebFilterFeaturePlugin::createConfigurationPage()
{
	return new WebFilterConfigurationPage(m_configuration);
}



IMPLEMENT_CONFIG_PROXY(WebFilterConfiguration)
