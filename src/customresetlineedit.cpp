/***************************************************************************
  customresetlineedit.cpp
  -------------------
  Copyright © 2014, LI-COR Biosciences, Antonio Forgione
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

#include "customresetlineedit.h"

#include <QAction>
#include <QIcon>

CustomResetLineEdit::CustomResetLineEdit(QWidget *parent) :
    CustomLineEdit(parent),
    stdText_(QString())
{
    setDefaultIcon(QIcon(QStringLiteral(":/icons/reset-line")));

    connect(action(), &QAction::triggered,
            this, [=](){ setText(stdText_); });
}

void CustomResetLineEdit::setDefaultText(const QString &text)
{
    stdText_ = text;
    setText(stdText_);
    action()->setVisible(false);
}

// NOTE: never used
QString CustomResetLineEdit::defaultText() const
{
    return stdText_;
}

void CustomResetLineEdit::updateAction(const QString &text)
{
    bool isTextChanged = (text != stdText_);

    action()->setVisible(isTextChanged);

    // NOTE: without this the icon will be only grayed out
    if (isTextChanged)
    {
        setIcon(defaultIcon());
    }
    else
    {
        setIcon(QIcon());
    }
}
