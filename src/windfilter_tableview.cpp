/***************************************************************************
  windfilter_tableview.cpp
  ------------------------
  Copyright © 2026-    , ETH Zurich, Jonathan Muller

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
#include "windfilter_tableview.h"

#include <QMouseEvent>

WindFilterTableView::WindFilterTableView(QWidget *parent)
    : QTableView(parent)
{}

QSize WindFilterTableView::sizeHint() const
{
    return QSize(235, 470);
}

int WindFilterTableView::sizeHintForRow(int /*row*/) const
{
    return 17;
}

void WindFilterTableView::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex idx = indexAt(event->pos());
    if (!idx.isValid())
        clearSelection();
    QTableView::mousePressEvent(event);
}
