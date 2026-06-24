/***************************************************************************
  windfilter_tablemodel.cpp
  -------------------------
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
#include "windfilter_tablemodel.h"

#include <algorithm>

#include <QColor>
#include <QVariant>

WindFilterTableModel::WindFilterTableModel(QObject *parent,
                                           QList<SectorItem> *sectors)
    : QAbstractTableModel(parent)
    , sectors_(sectors)
{}

Qt::ItemFlags WindFilterTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QVariant WindFilterTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= sectors_->size())
        return QVariant();

    const SectorItem &item = sectors_->at(index.row());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case START_ANGLE: return item.startAngle_;
            case END_ANGLE:   return item.endAngle_;
            default: break;
        }
    }

    if (role == Qt::DecorationRole && index.column() == START_ANGLE)
        return item.color_;

    return QVariant();
}

QVariant WindFilterTableModel::headerData(int section,
                                           Qt::Orientation orientation,
                                           int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case START_ANGLE: return tr("Start (°)");
            case END_ANGLE:   return tr("End (°)");
            default: break;
        }
    } else {
        return section + 1;
    }

    return QVariant();
}

int WindFilterTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return sectors_->size();
}

int WindFilterTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return MaxColumns;
}

bool WindFilterTableModel::setData(const QModelIndex &index,
                                    const QVariant &value,
                                    int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;
    if (index.row() >= sectors_->size())
        return false;

    SectorItem &item = (*sectors_)[index.row()];
    bool ok = false;
    const double v = value.toDouble(&ok);
    if (!ok)
        return false;

    switch (index.column()) {
        case START_ANGLE:
            item.startAngle_ = std::clamp(v, MinDegree, MaxDegree);
            break;
        case END_ANGLE:
            item.endAngle_ = std::clamp(v, MinDegree, MaxDegree);
            break;
        default:
            return false;
    }

    emit dataChanged(index, index, {role});
    emit modified();
    return true;
}

bool WindFilterTableModel::setHeaderData(int /*section*/,
                                          Qt::Orientation /*orientation*/,
                                          const QVariant & /*value*/,
                                          int /*role*/)
{
    return false;
}

bool WindFilterTableModel::insertRows(int row, int count,
                                       const QModelIndex &parent)
{
    if (sectors_->size() + count > MaxSectors)
        return false;

    beginInsertRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        SectorItem newItem(startAngleToBeInserted(),
                           endAngleToBeInserted(),
                           QColor(170, 230, 255));
        sectors_->insert(row + i, newItem);
    }
    endInsertRows();
    emit modified();
    return true;
}

bool WindFilterTableModel::removeRows(int row, int count,
                                       const QModelIndex &parent)
{
    if (row < 0 || row + count > sectors_->size())
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i)
        sectors_->removeAt(row);
    endRemoveRows();
    emit modified();
    return true;
}

void WindFilterTableModel::clear()
{
    if (sectors_->isEmpty())
        return;
    beginResetModel();
    sectors_->clear();
    endResetModel();
    emit modified();
}

void WindFilterTableModel::flush()
{
    if (sectors_->isEmpty())
        return;
    emit dataChanged(index(0, 0),
                     index(sectors_->size() - 1, MaxColumns - 1));
}

double WindFilterTableModel::angleSum()
{
    double total = 0.0;
    for (const SectorItem &s : *sectors_)
        total += s.endAngle_ - s.startAngle_;
    return total;
}

double WindFilterTableModel::startAngleToBeInserted()
{
    if (sectors_->isEmpty())
        return MinDegree;
    return sectors_->last().endAngle_;
}

double WindFilterTableModel::endAngleToBeInserted()
{
    const double start = startAngleToBeInserted();
    return std::min(start + DefaultSectorWidth, MaxDegree);
}
