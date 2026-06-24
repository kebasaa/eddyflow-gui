/***************************************************************************
  customlineedit.cpp
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

#include "customlineedit.h"

#include <QAction>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

CustomLineEdit::CustomLineEdit(QWidget *parent) :
    QLineEdit(parent),
    action_(nullptr),
    icon_(QIcon()),
    defaultIcon_(QIcon()),
    re_(QString())
{
    action_ = addAction(icon_, QLineEdit::TrailingPosition);

    connect(action_, &QAction::triggered,
            this, &CustomLineEdit::buttonClicked);
    connect(this, &QLineEdit::textChanged,
            this, &CustomLineEdit::updateAction);
}

QIcon CustomLineEdit::icon() const
{
    return icon_;
}

void CustomLineEdit::setIcon(const QIcon &icon)
{
    icon_ = icon;
    action_->setIcon(icon);
}

QIcon CustomLineEdit::defaultIcon() const
{
    return defaultIcon_;
}

void CustomLineEdit::setDefaultIcon(const QIcon &icon)
{
    defaultIcon_ = icon;
}

// NOTE: never used
QString CustomLineEdit::regExp() const
{
    return re_;
}

void CustomLineEdit::setRegExp(const QString &s)
{
    // remove the current validator
    if (s.isEmpty())
    {
        setValidator(nullptr);
        return;
    }

    QRegularExpression re(s);
    setValidator(new QRegularExpressionValidator(re, this));

    re_ = s;
}

QAction *CustomLineEdit::action() const
{
    return action_;
}
