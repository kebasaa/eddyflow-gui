/***************************************************************************
  mymenu.cpp
  -------------------
  Copyright © 2013-2018, LI-COR Biosciences, Antonio Forgione
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

#include "mymenu.h"

#include <QAction>
#include <QEvent>
#include <QHelpEvent>
#include <QToolTip>

MyMenu::MyMenu(QWidget* parent) : QMenu(parent)
{
}

bool MyMenu::event(QEvent* e)
{
//    if (QEvent::ToolTip == e->type())
//    {
//        // show action tooltip instead of widget tooltip
//        QHelpEvent *he = static_cast<QHelpEvent*>(e);
//        QAction *action = he ? actionAt(he->pos()) : nullptr;
//        if (action)
//        {
//            if (action->property("show-tooltip").toBool())
//            {
//                QToolTip::showText(he->globalPos(), action->toolTip(), this);
//            }
//            else
//            {
//                QToolTip::hideText();
//            }
//            return true;
//        }
//    }
    return QMenu::event(e);
}
