/***************************************************************************
  customdroplineedit.h
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

#ifndef CUSTOMDROPLINEEDIT_H
#define CUSTOMDROPLINEEDIT_H

#include "customclearlineedit.h"

class QDropEvent;
class QDragEnterEvent;

class CustomDropLineEdit : public CustomClearLineEdit
{
    Q_OBJECT

public:
    using CustomClearLineEdit::CustomClearLineEdit;

    bool canBeFile() const;
    void setCanBeFile(bool on);

signals:
    void dropped(const QString &text);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    bool canBeFile_ = { false };
};

#endif  // CUSTOMDROPLINEEDIT_H

