/***************************************************************************
  customcombomodel.h
  -------------------
  Reimplement QStandardItemModel with disabled item feature
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

#ifndef CUSTOMCOMBOMODEL_H
#define CUSTOMCOMBOMODEL_H

#include <QStandardItemModel>

////////////////////////////////////////////////////////////////////////////////
/// \file src/customcombomodel.h
/// \brief
/// \version
/// \date
/// \author      Antonio Forgione
/// \note
/// \sa
/// \bug
/// \deprecated
/// \test
/// \todo
////////////////////////////////////////////////////////////////////////////////

/// \class CustomComboModel
/// \brief Class that reimplement QStandardItemModel with disabled item feature
class CustomComboModel : public QStandardItemModel
{
public:
    explicit CustomComboModel(QObject* parent = nullptr);
    Qt::ItemFlags flags(const QModelIndex& index) const;
};

#endif // CUSTOMCOMBOMODEL_H
