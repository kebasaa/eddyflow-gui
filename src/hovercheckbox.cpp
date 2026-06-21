/***************************************************************************
  hovercheckbox.cpp
  -------------------
  Checkbox with hover effect
  -------------------
  Copyright © 2016-2018, LI-COR Biosciences, Antonio Forgione
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

#include "hovercheckbox.h"

#include <QStyleOptionButton>
#include <QStylePainter>

HoverCheckBox::HoverCheckBox(QWidget *parent) :
    QCheckBox(parent)
{
    isHover = false;
    isPressed = false;
}

void HoverCheckBox::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)

    QStylePainter p(this);
    QStyleOptionButton opt;

    initStyleOption(&opt);

    if (isHover)
    {
        opt.state |= QStyle::State_MouseOver;
    }
    if (isPressed)
    {
        opt.state |= QStyle::State_Sunken;
    }
    p.drawControl(QStyle::CE_CheckBox, opt);
}

void HoverCheckBox::setHover()
{
    isHover = true;
    update();
}

void HoverCheckBox::setPressed()
{
    isPressed = true;
    update();
}

void HoverCheckBox::clearPressed()
{
    isPressed = false;
    update();
}

void HoverCheckBox::clearStates()
{
    isHover = false;
    isPressed = false;
    update();
}
