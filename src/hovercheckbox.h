/***************************************************************************
  hovercheckbox.h
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

#ifndef HOVERCHECKBOX_H
#define HOVERCHECKBOX_H

#include <QCheckBox>

class HoverCheckBox : public QCheckBox
{
    Q_OBJECT

public:
    explicit HoverCheckBox(QWidget *parent = nullptr);

private:
    bool isHover;
    bool isPressed;

protected:
    void paintEvent(QPaintEvent *e) override;

public slots:
    void setHover();
    void setPressed();
    void clearPressed();
    void clearStates();
};

#endif  // HOVERCHECKBOX_H

