/*
 * ComputerItemDelegate.cpp - implementation of ComputerItemDelegate

 * Copyright (c) 2025-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QAbstractItemView>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyle>
#include <QWidget>

#include "ComputerControlListModel.h"
#include "ComputerItemDelegate.h"
#include "FeatureManager.h"
#include "VeyonCore.h"


ComputerItemDelegate::ComputerItemDelegate(QObject* parent) :
	QStyledItemDelegate(parent)
{
	initFeaturePixmaps();
}



void ComputerItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	if (index.isValid() == false)
	{
		return;
	}

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setRenderHint(QPainter::SmoothPixmapTransform);

	const bool selected = option.state.testFlag(QStyle::State_Selected);
	const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
	const bool dark = VeyonCore::useDarkMode();

	const QRect card = option.rect.adjusted(CardPadding, CardPadding, -CardPadding, -CardPadding);
	const QRect imageRect(card.left(), card.top(), card.width(), qMax(0, card.height() - CaptionHeight));
	const QRect captionRect(card.left(), imageRect.bottom(), card.width(), CaptionHeight);

	const QColor cardBg = dark ? QColor(0x14, 0x1b, 0x2d) : QColor(0xff, 0xff, 0xff);
	const QColor captionBg = dark ? QColor(0x10, 0x18, 0x2b) : QColor(0xf8, 0xfa, 0xfc);
	QColor border = dark ? QColor(0x2a, 0x35, 0x50) : QColor(0xdb, 0xe4, 0xf0);
	if (selected)
	{
		border = QColor(0x22, 0xd3, 0xee);
	}
	else if (hovered)
	{
		border = QColor(0x08, 0x91, 0xb2);
	}

	QPainterPath cardPath;
	cardPath.addRoundedRect(card, CardRadius, CardRadius);
	painter->fillPath(cardPath, cardBg);
	painter->setPen(QPen(border, selected ? 2.0 : 1.0));
	painter->drawPath(cardPath);

	const QPixmap screenshot = decorationPixmap(index, imageRect.size());
	painter->save();
	QPainterPath imageClip;
	imageClip.addRoundedRect(QRectF(imageRect).adjusted(1, 1, -1, 1), CardRadius, CardRadius);
	painter->setClipPath(imageClip);
	if (screenshot.isNull() == false)
	{
		const auto scaled = screenshot.scaled(imageRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
		painter->drawPixmap(imageRect.center().x() - scaled.width() / 2,
							imageRect.center().y() - scaled.height() / 2,
							scaled);
	}
	else
	{
		painter->fillRect(imageRect, dark ? QColor(0x0f, 0x17, 0x2a) : QColor(0xe2, 0xe8, 0xf0));
	}
	painter->restore();

	painter->save();
	QPainterPath captionClip;
	captionClip.addRoundedRect(card, CardRadius, CardRadius);
	painter->setClipPath(captionClip);
	painter->fillRect(captionRect, captionBg);
	painter->restore();

	const auto controlInterface = index.model()->data(index, ComputerControlListModel::ControlInterfaceRole)
									  .value<ComputerControlInterface::Pointer>();
	const auto status = statusColor(controlInterface);
	painter->setBrush(status);
	painter->setPen(Qt::NoPen);
	painter->drawEllipse(QPoint(captionRect.left() + 14, captionRect.center().y()), 4, 4);

	QFont font = option.font;
	font.setWeight(QFont::DemiBold);
	painter->setFont(font);
	painter->setPen(dark ? QColor(0xe8, 0xee, 0xf8) : QColor(0x0f, 0x17, 0x2a));
	const auto title = index.data(Qt::DisplayRole).toString();
	const QRect titleRect = captionRect.adjusted(26, 0, -10, 0);
	painter->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
					  painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

	drawFeatureIcons(painter, imageRect.topLeft() + QPoint(8, 8), controlInterface);

	painter->restore();
}



QSize ComputerItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Q_UNUSED(index)

	QSize icon = option.decorationSize;
	if (const auto* view = qobject_cast<const QAbstractItemView *>(option.widget))
	{
		icon = view->iconSize();
	}

	if (icon.isEmpty())
	{
		icon = QSize(160, 90);
	}

	return {icon.width() + 2 * CardPadding + 2, icon.height() + CaptionHeight + 2 * CardPadding + 2};
}



void ComputerItemDelegate::initFeaturePixmaps()
{
	for (const auto& feature : VeyonCore::featureManager().features() )
	{
		if (feature.testFlag(Feature::Flag::Master) && !feature.iconUrl().isEmpty())
		{
			m_featurePixmaps[feature.uid()] = QIcon(feature.iconUrl()).pixmap(QSize(OverlayIconSize, OverlayIconSize));
		}
	}
}



void ComputerItemDelegate::drawFeatureIcons(QPainter* painter, const QPoint& pos, ComputerControlInterface::Pointer controlInterface) const
{
	if (painter &&
		controlInterface &&
		controlInterface->state() == ComputerControlInterface::State::Connected)
	{
		auto count = 0;
		for (const auto& feature : controlInterface->activeFeatures())
		{
			if (m_featurePixmaps.contains(feature))
			{
				count++;
			}
		}

		if (count == 0)
		{
			return;
		}

		int x = pos.x();
		const int y = pos.y();
		const int chipWidth = count * (OverlayIconSize + OverlayIconSpacing) + OverlayIconsPadding;
		const int chipHeight = OverlayIconSize + OverlayIconsPadding;

		painter->setRenderHint(QPainter::Antialiasing);
		painter->setBrush(QColor(11, 16, 32, 210));
		painter->setPen(QColor(34, 211, 238));
		painter->drawRoundedRect(QRect(x, y, chipWidth, chipHeight),
								 OverlayIconsRadius, OverlayIconsRadius);

		x += OverlayIconsPadding / 2;
		for (const auto& feature : controlInterface->activeFeatures())
		{
			const auto it = m_featurePixmaps.find(feature);
			if (it != m_featurePixmaps.constEnd())
			{
				painter->drawPixmap(QPoint(x, y + OverlayIconsPadding / 2), *it);
				x += OverlayIconSize + OverlayIconSpacing;
			}
		}
	}
}



QPixmap ComputerItemDelegate::decorationPixmap(const QModelIndex& index, const QSize& size) const
{
	const auto decoration = index.data(Qt::DecorationRole);
	if (decoration.canConvert<QImage>())
	{
		const auto image = decoration.value<QImage>();
		if (image.isNull() == false)
		{
			return QPixmap::fromImage(image);
		}
	}

	if (decoration.canConvert<QPixmap>())
	{
		return decoration.value<QPixmap>();
	}

	if (decoration.canConvert<QIcon>())
	{
		return decoration.value<QIcon>().pixmap(size);
	}

	return {};
}



QColor ComputerItemDelegate::statusColor(const ComputerControlInterface::Pointer& controlInterface) const
{
	if (controlInterface.isNull())
	{
		return QColor(0x94, 0xa3, 0xb8);
	}

	switch (controlInterface->state())
	{
	case ComputerControlInterface::State::Connected:
		return QColor(0x34, 0xd3, 0x99);
	case ComputerControlInterface::State::AuthenticationFailed:
	case ComputerControlInterface::State::AccessControlFailed:
	case ComputerControlInterface::State::ServerNotRunning:
	case ComputerControlInterface::State::HostNameResolutionFailed:
		return QColor(0xf4, 0x3f, 0x5e);
	default:
		break;
	}

	return QColor(0x94, 0xa3, 0xb8);
}
