/***************************************************************************
  variable_model.h
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

#ifndef VARIABLE_MODEL_H
#define VARIABLE_MODEL_H

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QStringList>
#include <QVariant>

#include "variable_desc.h" // NOTE: for VariableDescList, maybe to fix

class VariableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // variable columns definition
    enum VarItem
    {
        IGNORE,
        NUMERIC,
        VARIABLE,
        INSTRUMENT,
        MEASURETYPE,
        INPUTUNIT,
        CONVERSIONTYPE,
        OUTPUTUNIT,
        AVALUE,
        BVALUE,
        NOMTIMELAG,
        MINTIMELAG,
        MAXTIMELAG,
        VARNUMCOLS
    };

    VariableModel(QObject *parent, VariableDescList *list);
    ~VariableModel();

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex& index,
                 const QVariant& value,
                 int role = Qt::EditRole);
    Qt::ItemFlags flags(const QModelIndex& index) const;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    bool insertColumns(int column,
                       int count,
                       const QModelIndex& parent = QModelIndex());
    bool removeColumns(int column,
                       int count,
                       const QModelIndex& parent = QModelIndex());
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    void flush();

    const QStringList instrModels() const;
    void setInstrModels(const QStringList& list);

signals:
    void modified();

private:
    VariableDescList *list_;
    QStringList instrModelList_;

private slots:
    void triggerSetData() const;
};

#endif // VARIABLE_MODEL_H
