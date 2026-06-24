/***************************************************************************
  tooltipfilter.cpp
  -------------------
  Filter to catch tooltips
  -------------------
  Copyright © 2007-2011, Eco2s team, Antonio Forgione
  Copyright © 2011-2018, LI-COR Biosciences, Antonio Forgione
  Copyright © 2026,      ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.

  EddyFlow® contains additional Open Source Components. The licenses
  and/or notices these Components can be found in the file LIBRARIES.txt.

  EddyFlow® is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
****************************************************************************/

#include "tooltipfilter.h"

#include <QDebug>
#include <QEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QAbstractItemView>

TooltipFilter::TooltipFilter(bool tooltipOn, QObject *parent)
    : QObject(parent),
      tooltipOn_(tooltipOn),
      tooltipType_(TooltipType::StdTooltip)
{
    if (!tooltipOn_)
        tooltipType_ = TooltipType::NoTooltip;
}

bool TooltipFilter::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QWidget *w = static_cast<QWidget *>(obj);
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
        QPoint pos = helpEvent->globalPos();
        QString itemTooltip = w->toolTip();
        QRect rect = w->rect();

        // case of instrument tables inside a scrollarea viewport
        if (w->objectName() == QLatin1String("qt_scrollarea_viewport"))
        {
            QAbstractItemView* view = qobject_cast<QAbstractItemView*>(obj->parent());
            if (!view)
            {
                return false;
            }
            w = view;

            QPoint position = helpEvent->pos();

            QModelIndex index = view->indexAt(position);
            if (!index.isValid())
            {
                return false;
            }
            rect = view->visualRect(index);
            itemTooltip = view->model()->data(index, Qt::ToolTipRole).toString();
        }

        switch (tooltipType_)
        {
            case TooltipType::NoTooltip:
                QToolTip::hideText();
                return true;
            case TooltipType::StdTooltip:
                QToolTip::showText(pos, itemTooltip, w, rect);
                return true;
            case TooltipType::CustomTooltip:
                break;
            default:
                QToolTip::hideText();
                emit updateTooltipRequest(itemTooltip);
                return true;
        }
    }

    return QObject::eventFilter(obj, event);
}

void TooltipFilter::setTooltipAvailable(bool available)
{
    tooltipOn_ = available;

    if (tooltipOn_)
        tooltipType_ = TooltipType::StdTooltip;
    else
        tooltipType_ = TooltipType::NoTooltip;
}
