/***************************************************************************
  irga_model.cpp
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

#include "irga_model.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QDebug>

#include "defs.h"
#include "dlproject.h"
#include "stringutils.h"
#include "widget_utils.h"

IrgaModel::IrgaModel(QObject *parent, IrgaDescList *list, DlProject *project) :
    QAbstractTableModel(parent),
    list_(list),
    project_(project)
{;}

qreal IrgaModel::stationAcFreq() const
{
    return project_ ? project_->acquisitionFrequency() : 10.0;
}

/// Whether the sampling choice changes anything for this instrument.
///
/// Only an instrument genuinely SLOWER than the station has an interval to
/// average over. The engine clamps the rate it uses to
/// min(instr%ac_freq, Metadata%ac_freq) (column_sampling.f90), so at or above
/// the station's rate the two choices produce the same pairing and the stored
/// value is never read.
bool IrgaModel::samplingIsRelevant(const IrgaDesc& irga) const
{
    return irga.acFreq() > 0.0 && irga.acFreq() < stationAcFreq();
}

IrgaModel::~IrgaModel(){
}

// Reset the model
void IrgaModel::flush()
{
    beginResetModel();
    endResetModel();
}

// Return data at index
QVariant IrgaModel::data(const QModelIndex& index, int role) const
{
    // row is the irga field
    int row = index.row();

    // column is the entry in the list
    int column = index.column();

    if (!index.isValid()) return QVariant();
    if (column >= list_->count()) return QVariant();

    const IrgaDesc& irgaDesc = list_->at(column);

    QVariant nullStrValue = QString();

    if (role == Qt::DisplayRole)
    {
        switch (row)
        {
            case MANUFACTURER:
                return QVariant(irgaDesc.manufacturer());
            case MODEL:
                if (irgaDesc.manufacturer() == IrgaDesc::getIRGA_MANUFACTURER_STRING_0())
                {
                    if (StringUtils::stringBelongsToList(irgaDesc.model(), IrgaDesc::licorModelStringList()))
                    {
                        return QVariant(irgaDesc.model());
                    }
                    else
                    {
                        const_cast<IrgaModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else if (irgaDesc.manufacturer() == IrgaDesc::getIRGA_MANUFACTURER_STRING_2())
                {
                    if (StringUtils::stringBelongsToList(irgaDesc.model(), IrgaDesc::campbellIrgaModelStringList()))
                    {
                        return QVariant(irgaDesc.model());
                    }
                    else
                    {
                        const_cast<IrgaModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else if (irgaDesc.manufacturer() == IrgaDesc::getIRGA_MANUFACTURER_STRING_3())
                {
                    if (StringUtils::stringBelongsToList(irgaDesc.model(), IrgaDesc::miroModelStringList()))
                    {
                        return QVariant(irgaDesc.model());
                    }
                    else
                    {
                        const_cast<IrgaModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else if (irgaDesc.manufacturer() == IrgaDesc::getIRGA_MANUFACTURER_STRING_4())
                {
                    if (StringUtils::stringBelongsToList(irgaDesc.model(), IrgaDesc::aerodyneModelStringList()))
                    {
                        return QVariant(irgaDesc.model());
                    }
                    else
                    {
                        const_cast<IrgaModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else
                {
                    if (StringUtils::stringBelongsToList(irgaDesc.model(), IrgaDesc::otherModelStringList()))
                    {
                        return QVariant(irgaDesc.model());
                    }
                    else
                    {
                        const_cast<IrgaModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
            case SWVERSION:
                return QVariant(irgaDesc.swVersion());
            case ID:
                return QVariant(irgaDesc.id());
            case TUBELENGTH:
                if (IrgaDesc::isOpenPathModel(irgaDesc.model()))
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(QString::number(irgaDesc.tubeLength(), 'f', 1) + QStringLiteral(" [cm]"));
                }
            case TUBEDIAMETER:
                if (IrgaDesc::isOpenPathModel(irgaDesc.model()))
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(QString::number(irgaDesc.tubeDiameter(), 'f', 1) + QStringLiteral(" [mm]"));
                }
            case TUBEFLOWRATE:
                if (IrgaDesc::isOpenPathModel(irgaDesc.model()))
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(QString::number(irgaDesc.tubeFlowRate(), 'f', 1) + QStringLiteral(" [l/m]"));
                }
            case TUBENSEPARATION:
                return QVariant(QString::number(irgaDesc.tubeNSeparation(), 'f', 2) + QStringLiteral(" [cm]"));
            case TUBEESEPARATION:
                return QVariant(QString::number(irgaDesc.tubeESeparation(), 'f', 2) + QStringLiteral(" [cm]"));
            case TUBEVSEPARATION:
                return QVariant(QString::number(irgaDesc.tubeVSeparation(), 'f', 2) + QStringLiteral(" [cm]"));
            case VPATHLENGTH:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_6()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_7()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_15()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_16()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_17()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_18()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_19()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_21()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_22()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_23()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_20())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(QString::number(irgaDesc.vPathLength(), 'f', 4) + QStringLiteral(" [cm]"));
                }
            case HPATHLENGTH:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_6()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_7()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_15()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_16()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_17()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_18()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_19()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_21()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_22()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_23()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_20())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(QString::number(irgaDesc.hPathLength(), 'f', 4) + QStringLiteral(" [cm]"));
                }
            case TAU:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_6()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_7()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_15()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_16()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_17()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_18()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_19()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_21()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_22()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_23()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_20())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(QString::number(irgaDesc.tau(), 'f', 4) + QStringLiteral(" [s]"));
                }
            case KWATER:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(QString::number(irgaDesc.kWater(), 'f', 6) + QStringLiteral(" [") + Defs::M3_G_CM_STRING + QStringLiteral("]"));
                }
            case KOXYGEN:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(QString::number(irgaDesc.kOxygen(), 'f', 6) + QStringLiteral(" [") + Defs::M3_G_CM_STRING + QStringLiteral("]"));
                }
            case ACFREQ:
                //> Named, not just resolved. The stored 0 means "follow the
                //> station", and a cell that printed only the station's number
                //> gave no way to tell that apart from an instrument pinned to
                //> the same rate - which is the state the editor then had no
                //> way back to.
                if (irgaDesc.acFreq() <= 0.0)
                {
                    return QVariant(tr("Station frequency (%1 Hz)")
                                    .arg(QString::number(stationAcFreq(), 'f', 3)));
                }
                return QVariant(QString::number(irgaDesc.acFreq(), 'f', 3)
                                + QStringLiteral(" [Hz]"));
            case SAMPLING:
                //> Always shown, and always editable - see flags(). It only
                //> MEANS anything for an instrument slower than the station,
                //> and that is said by grey text and a tooltip rather than by
                //> withholding the cell.
                return QVariant(irgaDesc.sampling());
            default:
                return QVariant();
        }
    }
    else if (role == Qt::EditRole)
    {
        switch (row)
        {
            case MANUFACTURER:
                return QVariant(irgaDesc.manufacturer());
            case MODEL:
                return QVariant(irgaDesc.model());
            case SWVERSION:
                return QVariant(irgaDesc.swVersion());
            case ID:
                return QVariant(irgaDesc.id());
            case TUBELENGTH:
                if (IrgaDesc::isOpenPathModel(irgaDesc.model()))
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(irgaDesc.tubeLength());
                }
            case TUBEDIAMETER:
                if (IrgaDesc::isOpenPathModel(irgaDesc.model()))
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(irgaDesc.tubeDiameter());
                }
            case TUBEFLOWRATE:
                if (IrgaDesc::isOpenPathModel(irgaDesc.model()))
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(irgaDesc.tubeFlowRate());
                }
            case TUBENSEPARATION:
                return QVariant(irgaDesc.tubeNSeparation());
            case TUBEESEPARATION:
                return QVariant(irgaDesc.tubeESeparation());
            case TUBEVSEPARATION:
                return QVariant(irgaDesc.tubeVSeparation());
            case VPATHLENGTH:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_6()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_7()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_17()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_18()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_19()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_21()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_22()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_23()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_20())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(irgaDesc.vPathLength());
                }
            case HPATHLENGTH:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_6()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_7()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_17()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_18()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_19()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_21()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_22()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_23()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_20())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(irgaDesc.hPathLength());
                }
            case TAU:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_6()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_7()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_17()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_18()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_19()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_21()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_22()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_23()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_20())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(irgaDesc.tau());
                }
            case KWATER:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(irgaDesc.kWater());
                }
            case KOXYGEN:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(irgaDesc.kOxygen());
                }
            case ACFREQ:
                //> The stored value, not the resolved one: an instrument that
                //> follows the station must open its editor on 0, which is the
                //> spin's special "Station frequency" entry. Seeding it with
                //> the station's number instead left no way back to 0.
                return QVariant(irgaDesc.acFreq());
            case SAMPLING:
                return QVariant(irgaDesc.sampling());
            default:
                return QVariant();
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (row)
        {
            case MANUFACTURER:
            case MODEL:
            case SWVERSION:
            case ID:
            case TUBELENGTH:
            case TUBEDIAMETER:
            case TUBEFLOWRATE:
            case TUBENSEPARATION:
            case TUBEESEPARATION:
            case TUBEVSEPARATION:
            case VPATHLENGTH:
            case HPATHLENGTH:
            case TAU:
            case KWATER:
            case KOXYGEN:
            case ACFREQ:
            case SAMPLING:
            default:
                return QVariant(Qt::AlignVCenter | Qt::AlignRight);
        }
    }
    else if (role == Qt::BackgroundRole)
    {
        switch (row)
        {
            case TUBELENGTH:
            case TUBEDIAMETER:
            case TUBEFLOWRATE:
                if (IrgaDesc::isOpenPathModel(irgaDesc.model()))
                {
                    return QVariant(QBrush(QColor(QStringLiteral("#eff0f1"))));
                }
                else
                {
                    return QVariant(QColor(Qt::white));
                }
            case VPATHLENGTH:
            case HPATHLENGTH:
            case TAU:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_6()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_7()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_17()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_18()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_19()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_21()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_22()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_23()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_20())
                {
                    return QVariant(QBrush(QColor(QStringLiteral("#eff0f1"))));
                }
                else
                {
                    return QVariant(QColor(Qt::white));
                }
            case KWATER:
            case KOXYGEN:
                if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                    && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11())
                {
                    return QVariant(QBrush(QColor(QStringLiteral("#eff0f1"))));
                }
                else
                {
                    return QVariant(QColor(Qt::white));
                }
            default:
                return QVariant(QColor(Qt::white));
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        switch (row)
        {
            case ACFREQ:
                if (irgaDesc.acFreq() <= 0.0)
                {
                    return QVariant(tr("Same as the station's acquisition frequency"));
                }
                return QVariant(tr("Acquisition frequency of this gas analyzer"));
            case SAMPLING:
                if (!samplingIsRelevant(irgaDesc))
                {
                    return QVariant(tr("Not relevant at the station's own "
                                       "acquisition frequency: there is no "
                                       "interval to average over. Stored, but "
                                       "not used."));
                }
                return QVariant(tr("Whether this instrument reports the value "
                                   "at an instant or the mean over its own "
                                   "sampling interval."));
            default:
                return QVariant();
        }
    }
    else if (role == Qt::ForegroundRole)
    {
        //> Greyed rather than withheld: the value is still there, still
        //> editable, and still saved - it just has no effect at this rate.
        if (row == SAMPLING && !samplingIsRelevant(irgaDesc))
        {
            return QVariant(QColor(Qt::darkGray));
        }
        return QVariant(QColor(Qt::black));
    }
    else
    {
        return QVariant();
    }
}

// Set data at index
bool IrgaModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    int row = index.row();
    int column = index.column();

    if (!index.isValid()) return false;
    if (role != Qt::EditRole) return false;
    if (column >= list_->count()) return false;

    // grab existing irga desc
    IrgaDesc irgaDesc = list_->value(column);

    switch (row)
    {
        case MANUFACTURER:
            // update only if new value is not equal to the old value
            if (value == irgaDesc.manufacturer())
            {
                return false;
            }
            irgaDesc.setManufacturer(value.toString());
            break;
        case MODEL:
            if (value == irgaDesc.model())
            {
                return false;
            }
            irgaDesc.setModel(value.toString());
            // Auto-fill source-backed analyzer constants. Setup-specific tube
            // and separation metadata remain user-supplied unless documented.
            if (value.toString() == IrgaDesc::getIRGA_MODEL_STRING_15())
            {
                irgaDesc.setVPathLength(15.31);
                irgaDesc.setHPathLength(0.64);
                irgaDesc.setTau(0.1);
            }
            else if (value.toString() == IrgaDesc::getIRGA_MODEL_STRING_16())
            {
                irgaDesc.setVPathLength(15.31);
                irgaDesc.setHPathLength(0.64);
                irgaDesc.setTau(0.1);
                irgaDesc.setTubeNSeparation(0.01);
                irgaDesc.setTubeESeparation(0.01);
                irgaDesc.setTubeVSeparation(0.01);
            }
            else if (value.toString() == IrgaDesc::getIRGA_MODEL_STRING_22())
            {
                irgaDesc.setVPathLength(11.77);
                irgaDesc.setHPathLength(0.80);
                irgaDesc.setTau(0.018);
            }
            else if (value.toString() == IrgaDesc::getIRGA_MODEL_STRING_23())
            {
                irgaDesc.setVPathLength(146.0);
                irgaDesc.setHPathLength(1.27);
                irgaDesc.setTau(0.023);
            }
            break;
        case SWVERSION:
            if (value == irgaDesc.swVersion())
            {
                return false;
            }
            irgaDesc.setSwVersion(value.toString());
            break;
        case ID:
            if (value == irgaDesc.id())
            {
                return false;
            }
            irgaDesc.setId(value.toString());
            break;
        case TUBELENGTH:
            if (value == irgaDesc.tubeLength())
            {
                return false;
            }
            irgaDesc.setTubeLength(value.toReal());
            break;
        case TUBEDIAMETER:
            if (value == irgaDesc.tubeDiameter())
            {
                return false;
            }
            irgaDesc.setTubeDiameter(value.toReal());
            break;
        case TUBEFLOWRATE:
            if (value == irgaDesc.tubeFlowRate())
            {
                return false;
            }
            irgaDesc.setTubeFlowRate(value.toReal());
            break;
        case TUBENSEPARATION:
            if (value == irgaDesc.tubeNSeparation())
            {
                return false;
            }
            irgaDesc.setTubeNSeparation(value.toReal());
            break;
        case TUBEESEPARATION:
            if (value == irgaDesc.tubeESeparation())
            {
                return false;
            }
            irgaDesc.setTubeESeparation(value.toReal());
            break;
        case TUBEVSEPARATION:
            if (value == irgaDesc.tubeVSeparation())
            {
                return false;
            }
            irgaDesc.setTubeVSeparation(value.toReal());
            break;
        case VPATHLENGTH:
            if (value == irgaDesc.vPathLength())
            {
                return false;
            }
            irgaDesc.setVPathLength(value.toReal());
            break;
        case HPATHLENGTH:
            if (value == irgaDesc.hPathLength())
            {
                return false;
            }
            irgaDesc.setHPathLength(value.toReal());
            break;
        case TAU:
            if (value == irgaDesc.tau())
            {
                return false;
            }
            irgaDesc.setTau(value.toReal());
            break;
        case KWATER:
            if (value == irgaDesc.kWater())
            {
                return false;
            }
            irgaDesc.setKWater(value.toReal());
            break;
        case KOXYGEN:
            if (value == irgaDesc.kOxygen())
            {
                return false;
            }
            irgaDesc.setKOxygen(value.toReal());
            break;
        case ACFREQ:
        {
            //> Stored as 0 when it matches the station, so the analyser goes on
            //> following the station rather than freezing today's number.
            const auto entered = value.toReal();
            //> <= 0 is the user asking for it outright, by winding the spin
            //> down to its "Station frequency" entry or typing a zero.
            const auto stored = (entered <= 0.0
                                 || qFuzzyCompare(entered, stationAcFreq()))
                                ? 0.0 : entered;
            //> Offset by one: qFuzzyCompare is undefined against exactly zero,
            //> which is the value that means "follow the station".
            if (qFuzzyCompare(stored + 1.0, irgaDesc.acFreq() + 1.0))
            {
                return false;
            }
            irgaDesc.setAcFreq(stored);
            break;
        }
        case SAMPLING:
            if (value == irgaDesc.sampling())
            {
                return false;
            }
            irgaDesc.setSampling(value.toString());
            break;
        default:
            return false;
    }

    list_->replace(column, irgaDesc);
    emit modified();

    // whole column may have changed
//    emit dataChanged(index.sibling(column, MANUFACTURER),
//                     index.sibling(column, KOXYGEN));
    emit dataChanged(index.sibling(MANUFACTURER, column),
                     index.sibling(SAMPLING, column));
    return true;
}

// Insert columns into table
bool IrgaModel::insertColumns(int column, int count, const QModelIndex& parent)
{
    Q_UNUSED(parent)
    if (count != 1) return false; // insert only one column at a time
    if ((column < 0) || (column >= list_->count())) column = list_->count();

    IrgaDesc irgaDesc = IrgaDesc();

    beginInsertColumns(QModelIndex(), column, column);
    list_->insert(column, irgaDesc);
    endInsertColumns();
    emit modified();
    return true;
}

// Remove columns from table
bool IrgaModel::removeColumns(int column, int count, const QModelIndex& parent)
{
    Q_UNUSED(parent)

    if (count != 1) return false; // only remove one column at a time
    if ((column < 0) || (column >= list_->count())) return false;

    if (!WidgetUtils::okToRemoveColumn(qApp->activeWindow()))
    {
        return false;
    }

    beginRemoveColumns(QModelIndex(), column, column);
    list_->removeAt(column);
    endRemoveColumns();
    emit modified();
    return true;
}

// Return header information
QVariant IrgaModel::headerData(int section, Qt::Orientation orientation,
                                int role) const
{
    if (orientation == Qt::Vertical && role == Qt::DisplayRole)
    {
        switch (section)
        {
            case MANUFACTURER:
            case MODEL:
            case SWVERSION:
            case ID:
            case TUBELENGTH:
            case TUBEDIAMETER:
            case TUBEFLOWRATE:
            case TUBENSEPARATION:
            case TUBEESEPARATION:
            case TUBEVSEPARATION:
            case VPATHLENGTH:
            case HPATHLENGTH:
            case TAU:
            case KWATER:
            case KOXYGEN:
            case ACFREQ:
            case SAMPLING:
                return QVariant(QString());
            default:
                return QVariant();
        }
    }

    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        return QVariant(section + 1);
    }

    if (orientation == Qt::Horizontal && role == Qt::BackgroundRole)
    {
        return QVariant(QBrush(QColor(Qt::darkGray)));
    }

    return QVariant();
}

// Return flags at index
Qt::ItemFlags IrgaModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::ItemIsEnabled;

    Qt::ItemFlags currentFlags = QAbstractTableModel::flags(index);
    currentFlags |= Qt::ItemIsEnabled;
    currentFlags |= Qt::ItemIsEditable;
    currentFlags |= Qt::ItemIsSelectable;

    int row = index.row();
    int column = index.column();

    const IrgaDesc& irgaDesc = list_->at(column);

    switch (row)
    {
        case TUBELENGTH:
        case TUBEDIAMETER:
        case TUBEFLOWRATE:
            if (IrgaDesc::isOpenPathModel(irgaDesc.model()))
            {
                currentFlags &= ~Qt::ItemIsEnabled;
                currentFlags &= ~Qt::ItemIsEditable;
                currentFlags &= ~Qt::ItemIsSelectable;
                return currentFlags;
            }
            else
            {
                return currentFlags;
            }
        case VPATHLENGTH:
        case HPATHLENGTH:
        case TAU:
            if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_6()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_7()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_17()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_18()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_19()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_21()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_22()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_23())
            {
                currentFlags &= ~Qt::ItemIsEnabled;
                currentFlags &= ~Qt::ItemIsEditable;
                currentFlags &= ~Qt::ItemIsSelectable;
                return currentFlags;
            }
            else
            {
                return currentFlags;
            }
        case KWATER:
        case KOXYGEN:
            if (irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_8()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_9()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_10()
                && irgaDesc.model() != IrgaDesc::getIRGA_MODEL_STRING_11())
            {
                currentFlags &= ~Qt::ItemIsEnabled;
                currentFlags &= ~Qt::ItemIsEditable;
                currentFlags &= ~Qt::ItemIsSelectable;
                return currentFlags;
            }
            else
            {
                return currentFlags;
            }
        case SAMPLING:
            //> Always selectable, on every instrument. It used to be greyed
            //> whenever the instrument had no rate of its own, which is also
            //> the state an instrument lands in the moment it is set to the
            //> station's rate - so stating the sampling once put the cell
            //> permanently out of reach. Whether the choice MATTERS is said by
            //> samplingIsRelevant(), in the foreground colour and the tooltip.
            return currentFlags;
        default:
            return currentFlags;
    }
}

// Return number of rows of data
int IrgaModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return IRGANUMCOLS;
}

// Return number of columns of data
int IrgaModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return list_->count();
}

