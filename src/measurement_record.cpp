/***************************************************************************
  measurement_record.cpp
  ----------------------
  Copyright © 2026-    , ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.
***************************************************************************/

#include "measurement_record.h"

#include <QCoreApplication>

#include <algorithm>

namespace {

const QString kH2o = QStringLiteral("h2o");

} // namespace

namespace MeasurementRecords {

int resolveMoistureRef(const QVector<GasRecord>& gases, int gasIndex)
{
    if (gasIndex < 0 || gasIndex >= gases.size()) { return 0; }

    const auto& gas = gases.at(gasIndex);

    // An explicit choice always wins, including one that crosses analysers:
    // correcting a gas with another instrument's H2O is the point of the
    // feature, not a mistake to be tidied away.
    if (gas.moistureRef > 0 && gas.moistureRef <= gases.size()
        && gases.at(gas.moistureRef - 1).slug == kH2o)
    {
        return gas.moistureRef;
    }

    // Otherwise the H2O on the same analyser.
    if (isRealInstrument(gas.instrumentId))
    {
        for (int i = 0; i < gases.size(); ++i)
        {
            if (gases.at(i).slug == kH2o
                && gases.at(i).instrumentId == gas.instrumentId)
            {
                return i + 1;
            }
        }
    }

    // Failing that, the first H2O the project has.
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).slug == kH2o) { return i + 1; }
    }

    return 0;
}

void validateReferences(QVector<GasRecord>& gases,
                        const QVector<MeasurementRecord>& cells)
{
    for (auto& gas : gases)
    {
        // Reset to auto rather than to some other record: auto re-resolves on
        // every read, so a project whose H2O was deleted repairs itself
        // instead of silently pointing at an unrelated gas.
        if (gas.moistureRef > gases.size()
            || (gas.moistureRef > 0
                && gases.at(gas.moistureRef - 1).slug != kH2o))
        {
            gas.moistureRef = 0;
        }
        if (gas.moistureRef < 0) { gas.moistureRef = 0; }

        if (gas.cellRef > cells.size() || gas.cellRef < 0) { gas.cellRef = 0; }
    }
}

QString gasLabel(const QVector<GasRecord>& gases, int index)
{
    if (index < 0 || index >= gases.size()) { return QString(); }

    const auto& gas = gases.at(index);
    auto text = gas.slug.toUpper();
    if (text.isEmpty())
    {
        // A project migrated from the old format leaves the fourth slot's
        // species blank until the metadata resolves it.
        text = QCoreApplication::translate("MeasurementRecords", "Gas %1")
                   .arg(index + 1);
    }
    if (isRealInstrument(gas.instrumentId))
    {
        text += QStringLiteral(" (") + gas.instrumentId + QStringLiteral(")");
    }
    return text;
}

QVector<int> gasDisplayOrder(const QVector<GasRecord>& gases)
{
    QVector<int> order;
    order.reserve(gases.size());
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).rawColumn > 0) { order.append(i); }
    }

    // Stable, so two records that label the same - the same species on the
    // same analyser, measured twice - keep record order between them.
    std::stable_sort(order.begin(), order.end(),
                     [&gases](int left, int right)
                     {
                         return QString::compare(gasLabel(gases, left),
                                                 gasLabel(gases, right),
                                                 Qt::CaseInsensitive) < 0;
                     });
    return order;
}

} // namespace MeasurementRecords
