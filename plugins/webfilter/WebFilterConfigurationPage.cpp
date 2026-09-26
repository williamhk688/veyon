/*
 * WebFilterConfigurationPage.cpp - edit school block/allow lists
 *
 * Copyright (c) 2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 */

#include <QInputDialog>
#include <QListWidget>
#include <QtAlgorithms>

#include "WebFilterConfiguration.h"
#include "WebFilterConfigurationPage.h"
#include "WebFilterLists.h"

#include "ui_WebFilterConfigurationPage.h"

WebFilterConfigurationPage::WebFilterConfigurationPage(WebFilterConfiguration& configuration, QWidget* parent) :
	ConfigurationPage(parent),
	ui(new Ui::WebFilterConfigurationPage),
	m_configuration(configuration)
{
	ui->setupUi(this);

	connect(ui->addBlockedButton, &QPushButton::clicked, this, &WebFilterConfigurationPage::addBlockedWebsite);
	connect(ui->removeBlockedButton, &QPushButton::clicked, this, &WebFilterConfigurationPage::removeBlockedWebsite);
	connect(ui->addAllowedButton, &QPushButton::clicked, this, &WebFilterConfigurationPage::addAllowedWebsite);
	connect(ui->removeAllowedButton, &QPushButton::clicked, this, &WebFilterConfigurationPage::removeAllowedWebsite);
}

WebFilterConfigurationPage::~WebFilterConfigurationPage()
{
	delete ui;
}

void WebFilterConfigurationPage::resetWidgets()
{
	ui->blockedList->clear();
	ui->allowedList->clear();
	ui->hardcodedList->clear();
	ui->blockedList->addItems(WebFilterLists::fromJson(m_configuration.blockedWebsites()));
	ui->allowedList->addItems(WebFilterLists::fromJson(m_configuration.allowedWebsites()));
	ui->hardcodedList->addItems(WebFilterLists::hardcodedProxyDomains() + WebFilterLists::hardcodedDohDomains());
}

void WebFilterConfigurationPage::connectWidgetsToProperties()
{
}

void WebFilterConfigurationPage::applyConfiguration()
{
	saveLists();
}

void WebFilterConfigurationPage::addBlockedWebsite()
{
	const auto domain = QInputDialog::getText(this, tr("加入不良網站"), tr("網域 (例如 example.com)"));
	const auto normalized = WebFilterLists::normalizeDomain(domain);
	if (normalized.isEmpty())
	{
		return;
	}
	if (ui->blockedList->findItems(normalized, Qt::MatchExactly).isEmpty())
	{
		ui->blockedList->addItem(normalized);
	}
	saveLists();
}

void WebFilterConfigurationPage::removeBlockedWebsite()
{
	qDeleteAll(ui->blockedList->selectedItems());
	saveLists();
}

void WebFilterConfigurationPage::addAllowedWebsite()
{
	const auto domain = QInputDialog::getText(this, tr("加入課堂網站"), tr("網域 (例如 classroom.google.com)"));
	const auto normalized = WebFilterLists::normalizeDomain(domain);
	if (normalized.isEmpty())
	{
		return;
	}
	if (WebFilterLists::isHardcodedBlocked(normalized))
	{
		return;
	}
	if (ui->allowedList->findItems(normalized, Qt::MatchExactly).isEmpty())
	{
		ui->allowedList->addItem(normalized);
	}
	saveLists();
}

void WebFilterConfigurationPage::removeAllowedWebsite()
{
	qDeleteAll(ui->allowedList->selectedItems());
	saveLists();
}

void WebFilterConfigurationPage::saveLists()
{
	QStringList blocked;
	for (int i = 0; i < ui->blockedList->count(); ++i)
	{
		blocked.append(ui->blockedList->item(i)->text());
	}
	QStringList allowed;
	for (int i = 0; i < ui->allowedList->count(); ++i)
	{
		allowed.append(ui->allowedList->item(i)->text());
	}
	m_configuration.setBlockedWebsites(WebFilterLists::toJson(blocked));
	m_configuration.setAllowedWebsites(WebFilterLists::toJson(
		WebFilterLists::effectiveAllowlist(allowed)));
}
