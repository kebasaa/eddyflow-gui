/***************************************************************************
  anem_model.cpp
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

#include "anem_model.h"

#include <QApplication>
#include <QDebug>

#include "defs.h"
#include "dlproject.h"
#include "stringutils.h"
#include "widget_utils.h"

AnemModel::AnemModel(QObject *parent, AnemDescList *list, DlProject *project) :
    QAbstractTableModel(parent),
    list_(list),
    project_(project)
{
}

qreal AnemModel::stationAcFreq() const
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
bool AnemModel::samplingIsRelevant(const AnemDesc& anem) const
{
    return anem.acFreq() > 0.0 && anem.acFreq() < stationAcFreq();
}

AnemModel::~AnemModel()
{
}

// Reset the model
void AnemModel::flush()
{
    beginResetModel();
    endResetModel();
}

// Return data at index
QVariant AnemModel::data(const QModelIndex& index, int role) const
{
    // row is the var field
    int row = index.row();
    int column = index.column();

    if (!index.isValid()) return QVariant();
    if (column >= list_->count()) return QVariant();

    // column is the entry in the QList
    const AnemDesc& anemDesc = list_->at(column);

    QVariant nullStrValue = QString();

    // row is the anem field
    if (role == Qt::DisplayRole)
    {
        switch (row)
        {
            case MANUFACTURER:
                return QVariant(anemDesc.manufacturer());
            case MODEL:
                if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_0())
                {
                    if (StringUtils::stringBelongsToList(anemDesc.model(),
                                                         AnemDesc::campbellModelStringList()))
                    {
                        return QVariant(anemDesc.model());
                    }
                    else
                    {
                        const_cast<AnemModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_1())
                {
                    if (StringUtils::stringBelongsToList(anemDesc.model(),
                                                         AnemDesc::gillModelStringList()))
                    {
                        return QVariant(anemDesc.model());
                    }
                    else
                    {
                        const_cast<AnemModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_2())
                {
                    if (StringUtils::stringBelongsToList(anemDesc.model(),
                                                         AnemDesc::metekModelStringList()))
                    {
                        return QVariant(anemDesc.model());
                    }
                    else
                    {
                        const_cast<AnemModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_3())
                {
                    if (StringUtils::stringBelongsToList(anemDesc.model(),
                                                         AnemDesc::youngModelStringList()))
                    {
                        return QVariant(anemDesc.model());
                    }
                    else
                    {
                        const_cast<AnemModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_4())
                {
                    if (anemDesc.model() == AnemDesc::getANEM_MODEL_STRING_12())
                    {
                        return QVariant(anemDesc.model());
                    }
                    else
                    {
                        const_cast<AnemModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else
                {
                    return QVariant(anemDesc.model());
                }
            case SWVERSION:
                return QVariant(anemDesc.swVersion());
            case ID:
                return QVariant(anemDesc.id());
            case HEIGHT:
                return QVariant(QString::number(anemDesc.height(), 'f', 2) + QStringLiteral(" [m]"));
            case WINDFORMAT:
                if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_2()
                    || anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_3())
                {
                    if (anemDesc.windFormat() == AnemDesc::getANEM_WIND_FORMAT_STRING_0()
                        || anemDesc.windFormat() == AnemDesc::getANEM_WIND_FORMAT_STRING_1())
                    {
                        return QVariant(anemDesc.windFormat());
                    }
                    else
                    {
                        const_cast<AnemModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_0())
                {
                    if (anemDesc.windFormat() == AnemDesc::getANEM_WIND_FORMAT_STRING_0())
                    {
                        return QVariant(anemDesc.windFormat());
                    }
                    else
                    {
                        const_cast<AnemModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_1())
                {
                    if (StringUtils::stringBelongsToList(anemDesc.windFormat(), AnemDesc::allWindFormatStringList()))
                    {
                        return QVariant(anemDesc.windFormat());
                    }
                    else
                    {
                        const_cast<AnemModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else
                {
                    return QVariant(anemDesc.windFormat());
                }
            case NORTHALIGNMENT:
                if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_0()
                    || anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_2()
                    || anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_3()
                    || anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_4())
                {
                    if (anemDesc.northAlignment() == AnemDesc::getANEM_NORTH_ALIGN_STRING_2())
                    {
                        return QVariant(anemDesc.northAlignment());
                    }
                    else
                    {
                        const_cast<AnemModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_1())
                {
                    if (anemDesc.northAlignment() == AnemDesc::getANEM_NORTH_ALIGN_STRING_0()
                        || anemDesc.northAlignment() == AnemDesc::getANEM_NORTH_ALIGN_STRING_1())
                    {
                        return QVariant(anemDesc.northAlignment());
                    }
                    else
                    {
                        const_cast<AnemModel *>(this)->setData(index, nullStrValue);
                        return nullStrValue;
                    }
                }
                else
                {
                    return QVariant(anemDesc.northAlignment());
                }
            case NORTHOFFSET:
                return QVariant(QString::number(anemDesc.northOffset(), 'f', 1) + tr(" [%1]").arg(Defs::DEGREE));
            case NSEPARATION:
                if (column == 0)
                    return QVariant(QStringLiteral("Reference"));
                else
                    return QVariant(QString::number(anemDesc.nSeparation(), 'f', 2) + QStringLiteral(" [cm]"));
            case ESEPARATION:
                if (column == 0)
                    return QVariant(QStringLiteral("Reference"));
                else
                    return QVariant(QString::number(anemDesc.eSeparation(), 'f', 2) + QStringLiteral(" [cm]"));
            case VSEPARATION:
                if (column == 0)
                    return QVariant(QStringLiteral("Reference"));
                else
                    return QVariant(QString::number(anemDesc.vSeparation(), 'f', 2) + QStringLiteral(" [cm]"));
            case VPATHLENGTH:
                if (anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_12()
                    && anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_19()
                    && anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_20()
                    && anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_21())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(QString::number(anemDesc.vPathLength(), 'f', 4) + QStringLiteral(" [cm]"));
                }
            case HPATHLENGTH:
                if (anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_12()
                    && anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_19()
                    && anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_20()
                    && anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_21())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(QString::number(anemDesc.hPathLength(), 'f', 4) + QStringLiteral(" [cm]"));
                }
            case TAU:
                if (anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_12()
                    && anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_19()
                    && anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_20()
                    && anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_21())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(QString::number(anemDesc.tau(), 'f', 4) + QStringLiteral(" [s]"));
                }
            case ACFREQ:
                //> Named, not just resolved. The stored 0 means "follow the
                //> station", and a cell that printed only the station's number
                //> gave no way to tell that apart from an instrument pinned to
                //> the same rate - which is the state the editor then had no
                //> way back to.
                if (anemDesc.acFreq() <= 0.0)
                {
                    return QVariant(tr("Station frequency (%1 Hz)")
                                    .arg(QString::number(stationAcFreq(), 'f', 3)));
                }
                return QVariant(QString::number(anemDesc.acFreq(), 'f', 3)
                                + QStringLiteral(" [Hz]"));
            case SAMPLING:
                //> Always shown, and always editable - see flags(). It only
                //> MEANS anything for an instrument slower than the station,
                //> and that is said by grey text and a tooltip rather than by
                //> withholding the cell.
                return QVariant(anemDesc.sampling());
            default:
                return QVariant();
        }
    }
    else if (role == Qt::EditRole)
    {
        switch (row)
        {
            case MANUFACTURER:
                return QVariant(anemDesc.manufacturer());
            case MODEL:
                if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_0())
                {
                    if (StringUtils::stringBelongsToList(anemDesc.model(),
                                                         AnemDesc::campbellModelStringList()))
                    {
                        return QVariant(anemDesc.model());
                    }
                    else
                    {
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_1())
                {
                    if (StringUtils::stringBelongsToList(anemDesc.model(),
                                                         AnemDesc::gillModelStringList()))
                    {
                        return QVariant(anemDesc.model());
                    }
                    else
                    {
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_2())
                {
                    if (StringUtils::stringBelongsToList(anemDesc.model(),
                                                         AnemDesc::metekModelStringList()))
                    {
                        return QVariant(anemDesc.model());
                    }
                    else
                    {
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_3())
                {
                    if (StringUtils::stringBelongsToList(anemDesc.model(),
                                                         AnemDesc::youngModelStringList()))
                    {
                        return QVariant(anemDesc.model());
                    }
                    else
                    {
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_4())
                {
                    if (anemDesc.model() == AnemDesc::getANEM_MODEL_STRING_12())
                    {
                        return QVariant(anemDesc.model());
                    }
                    else
                    {
                        return nullStrValue;
                    }
                }
                else
                {
                    return QVariant(anemDesc.model());
                }
            case SWVERSION:
                return QVariant(anemDesc.swVersion());
            case ID:
                return QVariant(anemDesc.id());
            case HEIGHT:
                return QVariant(anemDesc.height());
            case WINDFORMAT:
                if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_2()
                    || anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_3())
                {
                    if (anemDesc.windFormat() == AnemDesc::getANEM_WIND_FORMAT_STRING_0()
                        || anemDesc.windFormat() == AnemDesc::getANEM_WIND_FORMAT_STRING_1())
                    {
                        return QVariant(anemDesc.windFormat());
                    }
                    else
                    {
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_0())
                {
                    if (anemDesc.windFormat() == AnemDesc::getANEM_WIND_FORMAT_STRING_0())
                    {
                        return QVariant(anemDesc.windFormat());
                    }
                    else
                    {
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_1())
                {
                    if (StringUtils::stringBelongsToList(anemDesc.windFormat(), AnemDesc::allWindFormatStringList()))
                    {
                        return QVariant(anemDesc.windFormat());
                    }
                    else
                    {
                        return nullStrValue;
                    }
                }
                else
                {
                    return QVariant(anemDesc.windFormat());
                }
            case NORTHALIGNMENT:
                if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_0()
                    || anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_2()
                    || anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_3()
                    || anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_4())
                {
                    if (anemDesc.northAlignment() == AnemDesc::getANEM_NORTH_ALIGN_STRING_2())
                    {
                        return QVariant(anemDesc.northAlignment());
                    }
                    else
                    {
                        return nullStrValue;
                    }
                }
                else if (anemDesc.manufacturer() == AnemDesc::getANEM_MANUFACTURER_STRING_1())
                {
                    if (anemDesc.northAlignment() == AnemDesc::getANEM_NORTH_ALIGN_STRING_0()
                        || anemDesc.northAlignment() == AnemDesc::getANEM_NORTH_ALIGN_STRING_1())
                    {
                        return QVariant(anemDesc.northAlignment());
                    }
                    else
                    {
                        return nullStrValue;
                    }
                }
                else
                {
                    return QVariant(anemDesc.northAlignment());
                }
            case NORTHOFFSET:
                return QVariant(anemDesc.northOffset());
            case NSEPARATION:
                if (column == 0)
                {
                    return QVariant(QStringLiteral("Reference"));
                }
                else
                {
                    return QVariant(anemDesc.nSeparation());
                }
            case ESEPARATION:
                if (column == 0)
                {
                    return QVariant(QStringLiteral("Reference"));
                }
                else
                {
                    return QVariant(anemDesc.eSeparation());
                }
            case VSEPARATION:
                if (column == 0)
                {
                    return QVariant(QStringLiteral("Reference"));
                }
                else
                {
                    return QVariant(anemDesc.vSeparation());
                }
            case VPATHLENGTH:
                if (anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_12())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(anemDesc.vPathLength());
                }
            case HPATHLENGTH:
                if (anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_12())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(anemDesc.hPathLength());
                }
            case TAU:
                if (anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_12())
                {
                    return nullStrValue;
                }
                else
                {
                    return QVariant(anemDesc.tau());
                }
            case ACFREQ:
                //> The stored value, not the resolved one: an instrument that
                //> follows the station must open its editor on 0, which is the
                //> spin's special "Station frequency" entry. Seeding it with
                //> the station's number instead left no way back to 0.
                return QVariant(anemDesc.acFreq());
            case SAMPLING:
                return QVariant(anemDesc.sampling());
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
            case HEIGHT:
            case WINDFORMAT:
            case NORTHALIGNMENT:
            case NORTHOFFSET:
            case NSEPARATION:
            case ESEPARATION:
            case VSEPARATION:
            case VPATHLENGTH:
            case HPATHLENGTH:
            case TAU:
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
            case NSEPARATION:
            case ESEPARATION:
            case VSEPARATION:
                if (column == 0)
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
                if (anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_12())
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
    else if (role == Qt::ForegroundRole)
    {
        //> Greyed rather than withheld: the value is still there, still
        //> editable, and still saved - it just has no effect at this rate.
        if (row == SAMPLING && !samplingIsRelevant(anemDesc))
        {
            return QVariant(QColor(Qt::darkGray));
        }
        return QVariant(QColor(Qt::black));
    }
    // tooltips texts
    else if (role == Qt::ToolTipRole)
    {
        switch (row)
        {
            case MANUFACTURER:
                return QVariant(tr("Manufacturer value"));
            case MODEL:
                return QVariant(tr("Model value"));
            case SWVERSION:
                return QVariant(tr("Sw Version"));
            case ID:
                return QVariant(tr("ID value"));
            case HEIGHT:
                return QVariant(tr("Height value"));
            case WINDFORMAT:
                return QVariant(tr("Wind data format value"));
            case NORTHALIGNMENT:
                if (anemDesc.northAlignment() == QLatin1String("Axis"))
                {
                    return QVariant(tr("Axis alignment..."));
                }
                else if (anemDesc.northAlignment() == QLatin1String("Spar"))
                {
                    return QVariant(tr("Spar alignment..."));
                }
                else
                {
                    return QVariant(tr("North alignment possible value: Axis..., Spar..., N/A..."));
                }
            case NORTHOFFSET:
                return QVariant(tr("North off-set value"));
            case ACFREQ:
                if (anemDesc.acFreq() <= 0.0)
                {
                    return QVariant(tr("Same as the station's acquisition frequency"));
                }
                return QVariant(tr("Acquisition frequency of this anemometer"));
            case SAMPLING:
                if (!samplingIsRelevant(anemDesc))
                {
                    return QVariant(tr("Not relevant at the station's own "
                                       "acquisition frequency: there is no "
                                       "interval to average over. Stored, but "
                                       "not used."));
                }
                return QVariant(tr("Whether this instrument reports the value "
                                   "at an instant or the mean over its own "
                                   "sampling interval."));
            case VPATHLENGTH:
            case HPATHLENGTH:
            case TAU:
            default:
                return QVariant();
        }
    }

    return QVariant();
}

// Set data at index
bool AnemModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    int row = index.row();
    int column = index.column();

    if (!index.isValid()) return false;
    if (role != Qt::EditRole) return false;
    if (column >= list_->count()) return false;

    // grab existing anem desc
    AnemDesc anemDesc = list_->value(column);

    switch (row)
    {
        case MANUFACTURER:
            // update only if new value is not equal to the old value
            if (value == anemDesc.manufacturer())
            {
                return false;
            }
            anemDesc.setManufacturer(value.toString());
            break;
        case MODEL:
            if (value == anemDesc.model())
            {
                return false;
            }
            anemDesc.setModel(value.toString());
            // auto-fill known path lengths and time response
            if (value.toString() == AnemDesc::getANEM_MODEL_STRING_19()
                || value.toString() == AnemDesc::getANEM_MODEL_STRING_20())
            {
                anemDesc.setVPathLength(11.547);
                anemDesc.setHPathLength(0.8);
                anemDesc.setTau(0.05);
            }
            else if (value.toString() == AnemDesc::getANEM_MODEL_STRING_21())
            {
                anemDesc.setVPathLength(11.547);
                anemDesc.setHPathLength(0.64);
                anemDesc.setTau(0.05);
            }
            break;
        case SWVERSION:
            if (value == anemDesc.swVersion())
            {
                return false;
            }
            anemDesc.setSwVersion(value.toString());
            break;
        case ID:
            if (value == anemDesc.id())
            {
                return false;
            }
            anemDesc.setId(value.toString());
            break;
        case HEIGHT:
            if (value == anemDesc.height())
            {
                return false;
            }
            anemDesc.setHeight(value.toReal());
            break;
        case WINDFORMAT:
            if (value == anemDesc.windFormat())
            {
                return false;
            }
            anemDesc.setWindFormat(value.toString());
            break;
        case NORTHALIGNMENT:
            if (value == anemDesc.northAlignment())
            {
                return false;
            }
            anemDesc.setNorthAlignment(value.toString());
            break;
        case NORTHOFFSET:
            if (value == anemDesc.northOffset())
            {
                return false;
            }
            anemDesc.setNorthOffset(value.toReal());
            break;
        case NSEPARATION:
            if (value == anemDesc.nSeparation())
            {
                return false;
            }
            anemDesc.setNSeparation(value.toReal());
            break;
        case ESEPARATION:
            if (value == anemDesc.eSeparation())
            {
                return false;
            }
            anemDesc.setESeparation(value.toReal());
            break;
        case VSEPARATION:
            if (value == anemDesc.vSeparation())
            {
                return false;
            }
            anemDesc.setVSeparation(value.toReal());
            break;
        case VPATHLENGTH:
            if (value == anemDesc.vPathLength())
            {
                return false;
            }
            anemDesc.setVPathLength(value.toReal());
            break;
        case HPATHLENGTH:
            if (value == anemDesc.hPathLength())
            {
                return false;
            }
            anemDesc.setHPathLength(value.toReal());
            break;
        case TAU:
            if (value == anemDesc.tau())
            {
                return false;
            }
            anemDesc.setTau(value.toReal());
            break;
        case ACFREQ:
        {
            //> Stored as 0 when it matches the station, so the instrument goes
            //> on following the station rather than freezing today's number.
            //> Typing the station's own rate is not "changing it".
            const auto entered = value.toReal();
            //> <= 0 is the user asking for it outright, by winding the spin
            //> down to its "Station frequency" entry or typing a zero.
            const auto stored = (entered <= 0.0
                                 || qFuzzyCompare(entered, stationAcFreq()))
                                ? 0.0 : entered;
            //> Offset by one: qFuzzyCompare is undefined against exactly
            //> zero, which is the value that means "follow the station".
            if (qFuzzyCompare(stored + 1.0, anemDesc.acFreq() + 1.0))
            {
                return false;
            }
            anemDesc.setAcFreq(stored);
            break;
        }
        case SAMPLING:
            if (value == anemDesc.sampling())
            {
                return false;
            }
            anemDesc.setSampling(value.toString());
            break;
        default:
            return false;
    }

    list_->replace(column, anemDesc);
    emit modified();

    // whole column may have changed
    emit dataChanged(index.sibling(MANUFACTURER, column),
                     index.sibling(SAMPLING, column));
    return true;
}

// Insert columns into table
bool AnemModel::insertColumns(int column, int count, const QModelIndex& parent)
{
    Q_UNUSED(parent)
    if (count != 1) return false; // insert only one column at a time
    if ((column < 0) || (column >= list_->count()))
        column = list_->count();

    AnemDesc anemDesc = AnemDesc();

    beginInsertColumns(QModelIndex(), column, column);
    list_->insert(column, anemDesc);
    endInsertColumns();
    emit modified();
    return true;
}

// Remove columns from table
bool AnemModel::removeColumns(int column, int count, const QModelIndex& parent)
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
QVariant AnemModel::headerData(int section, Qt::Orientation orientation,
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
            case HEIGHT:
            case WINDFORMAT:
            case NORTHALIGNMENT:
            case NORTHOFFSET:
            case NSEPARATION:
            case ESEPARATION:
            case VSEPARATION:
            case VPATHLENGTH:
            case HPATHLENGTH:
            case TAU:
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
Qt::ItemFlags AnemModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::ItemIsEnabled;

    Qt::ItemFlags currentFlags = QAbstractTableModel::flags(index);
    currentFlags |= Qt::ItemIsEnabled;
    currentFlags |= Qt::ItemIsEditable;
    currentFlags |= Qt::ItemIsSelectable;

    int row = index.row();
    int column = index.column();

    AnemDesc anemDesc = list_->value(column);

    switch (row)
    {
        case NSEPARATION:
        case ESEPARATION:
        case VSEPARATION:
            if (column == 0)
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
            if (anemDesc.model() != AnemDesc::getANEM_MODEL_STRING_12())
            {
                currentFlags &= ~Qt::ItemIsEnabled;
                currentFlags &= ~Qt::ItemIsEditable;
                currentFlags &= ~Qt::ItemIsSelectable;
                return currentFlags;            }
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
int AnemModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return ANEMNUMCOLS;
}

// Return number of columns of data
int AnemModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return list_->count();
}

