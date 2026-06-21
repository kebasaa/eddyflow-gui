/***************************************************************************
  nonzerodoublespinbox.cpp
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

#include "nonzerodoublespinbox.h"

NonZeroDoubleSpinBox::NonZeroDoubleSpinBox(QWidget *parent)
    : QDoubleSpinBox(parent)
{
}

double NonZeroDoubleSpinBox::valueFromText(const QString &text) const
{
    double value = text.toDouble();

    if (value == 0.0)
    {
        return 0.000001;
    }
    return value;
}
