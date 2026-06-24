/***************************************************************************
  clicklabel.cpp
  -------------------
  QLabel class with clicked signal
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

#include "clicklabel.h"

#include <QApplication>

ClickLabel::ClickLabel(QWidget* parent, Qt::WindowFlags flags)
    : QLabel(parent, flags),
      headerData_(NoHeader)
{
}

ClickLabel::ClickLabel(const QString& text, QWidget* parent, Qt::WindowFlags flags)
    : QLabel(text, parent, flags),
      headerData_(NoHeader)
{
}

ClickLabel::ClickLabel(const ClickLabel& clabel)
    : QLabel(),
      headerData_(NoHeader)
{
    Q_UNUSED(clabel)
}

ClickLabel &ClickLabel::operator=(const ClickLabel &clabel)
{
    if (this != &clabel)
    {
        headerData_ = clabel.headerData_;
    }
    return *this;
}

ClickLabel::~ClickLabel()
{}

void ClickLabel::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
    time.restart();
}

void ClickLabel::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
    if (time.elapsed() < static_cast<qint64>(qApp->doubleClickInterval()))
        emit clicked();
}

void ClickLabel::setHeaderData(HeaderData data)
{
    headerData_ = data;
}
