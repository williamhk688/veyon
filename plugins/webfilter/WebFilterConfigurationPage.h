/*
 * WebFilterConfigurationPage.h - Configurator page for website lists
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#pragma once

#include "ConfigurationPage.h"

class WebFilterConfiguration;

namespace Ui {
class WebFilterConfigurationPage;
}

class WebFilterConfigurationPage : public ConfigurationPage
{
	Q_OBJECT
public:
	explicit WebFilterConfigurationPage(WebFilterConfiguration& configuration, QWidget* parent = nullptr);
	~WebFilterConfigurationPage() override;

	void resetWidgets() override;
	void connectWidgetsToProperties() override;
	void applyConfiguration() override;

private:
	void addBlockedWebsite();
	void removeBlockedWebsite();
	void addAllowedWebsite();
	void removeAllowedWebsite();
	void addExtraProxyWebsite();
	void removeExtraProxyWebsite();
	void saveLists();

	Ui::WebFilterConfigurationPage* ui;
	WebFilterConfiguration& m_configuration;
};
