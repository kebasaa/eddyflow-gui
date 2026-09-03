/***************************************************************************
  anem_model.h
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

#ifndef ANEM_MODEL_H
#define ANEM_MODEL_H

#include <QAbstractTableModel>
#include <QIcon>
#include <QModelIndex>
#include <QVariant>

#include "anem_desc.h" // NOTE: for AnemDescList

class DlProject;

class AnemModel : public QAbstractTableModel {
    Q_OBJECT
public:
    // model columns
    enum AnemItem
    {
        MANUFACTURER,
        MODEL,
        SWVERSION,
        ID,
        HEIGHT,
        WINDFORMAT,
        NORTHALIGNMENT,
        NORTHOFFSET,
        NSEPARATION,
        ESEPARATION,
        VSEPARATION,
        VPATHLENGTH,
        HPATHLENGTH,
        TAU,
        ACFREQ,
        SAMPLING,
        ANEMNUMCOLS
    };

    //> Takes the project as well as the list, because the acquisition
    //> frequency row shows the station's rate for any instrument that has not
    //> been given one of its own, and the station's rate lives in the project.
    AnemModel(QObject *parent, AnemDescList *list, DlProject *project);
    ~AnemModel();

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

signals:
    void modified();

private:
    //> The station's acquisition frequency, or DlProject's own 10 Hz default
    //> when there is no project to ask.
    qreal stationAcFreq() const;
    //> The rate to show for an instrument: its own when it states one, the
    //> station's while it does not.
    /// Whether the sampling choice changes anything at this instrument's rate.
    bool samplingIsRelevant(const AnemDesc& anem) const;

    AnemDescList *list_;
    DlProject *project_;
};

#endif // ANEM_MODEL_H

