/***************************************************************************
  customcheckbox.cpp
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
 ***************************************************************************/

#include "customcheckbox.h"

#include <QCheckBox>
#include <QHBoxLayout>

#include "clickablelabel.h"
#include "hovercheckbox.h"

CustomCheckBox::CustomCheckBox(QWidget *parent) : QWidget(parent)
{
    this->setAttribute(Qt::WA_Hover);

    checkbox = new HoverCheckBox;
    label = new ClickableLabel;

    auto container = new QHBoxLayout(this);
    container->addWidget(checkbox);
    container->addWidget(label);
    container->addStretch();
    container->setContentsMargins(0, 0, 11, 0);
    container->setSpacing(8);

    setLayout(container);

    connect(checkbox, &HoverCheckBox::toggled,
            this, &CustomCheckBox::toggled);

    connect(label, &ClickableLabel::released, checkbox, &QAbstractButton::toggle);
    connect(label, &ClickableLabel::released, checkbox, &HoverCheckBox::clearPressed);
    connect(label, &ClickableLabel::pressed, checkbox, &HoverCheckBox::setPressed);
    connect(label, &ClickableLabel::left, checkbox, &HoverCheckBox::clearStates);
    connect(label, &ClickableLabel::released, this, &CustomCheckBox::clicked);
}

CustomCheckBox::~CustomCheckBox()
{
}

// NOTE: never used
void CustomCheckBox::setLabel(const QString &text)
{
    label->setText(text);
}

void CustomCheckBox::setChecked(bool checked)
{
    checkbox->setChecked(checked);
}

void CustomCheckBox::enterEvent(QEnterEvent *event)
{
    checkbox->setHover();
    QWidget::enterEvent(event);
}

void CustomCheckBox::leaveEvent(QEvent *event)
{
    checkbox->clearStates();
    QWidget::leaveEvent(event);
}


