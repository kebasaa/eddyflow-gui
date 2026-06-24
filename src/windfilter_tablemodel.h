/***************************************************************************
  windfilter_tablemodel.h
  -----------------------
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
#ifndef WINDFILTER_TABLEMODEL_H
#define WINDFILTER_TABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>

#include "sector_item.h"

class WindFilterTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { START_ANGLE = 0, END_ANGLE = 1 };

    static constexpr int    MaxColumns        = 2;
    static constexpr double MinDegree         = 0.0;
    static constexpr double MaxDegree         = 360.0;
    static constexpr double DefaultSectorWidth = 10.0;
    static constexpr int    MaxSectors        = 16;

    WindFilterTableModel(QObject *parent, QList<SectorItem> *sectors);

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant      data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant      headerData(int section, Qt::Orientation orientation,
                             int role = Qt::DisplayRole) const override;
    int           rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int           columnCount(const QModelIndex &parent = QModelIndex()) const override;

    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    bool setHeaderData(int section, Qt::Orientation orientation,
                       const QVariant &value, int role = Qt::EditRole) override;

    bool insertRows(int row, int count,
                    const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count,
                    const QModelIndex &parent = QModelIndex()) override;

    void   clear();
    void   flush();
    double angleSum();

signals:
    void modified();

private:
    double startAngleToBeInserted();
    double endAngleToBeInserted();

    QList<SectorItem> *sectors_;
};

#endif // WINDFILTER_TABLEMODEL_H
