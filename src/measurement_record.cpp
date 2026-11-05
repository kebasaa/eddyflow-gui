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

int resolveMoistureRef(const QVector<GasRecord>& gases, int gasIndex,
                       bool biometRhAvailable)
{
    if (gasIndex < 0 || gasIndex >= gases.size()) { return 0; }

    const auto& gas = gases.at(gasIndex);

    // The biomet, named. Honoured whether or not the project also has a
    // hygrometer - saying so is how a user chooses between them.
    if (gas.moistureRef == biometMoistureRef && biometRhAvailable)
    {
        return biometMoistureRef;
    }

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

    // Failing that, the biomet.
    //
    // This step used to be "the first H2O the project has", which handed a gas
    // whose own analyser carries no hygrometer another instrument's water,
    // taken through a different cell at a different time lag. A site RH sensor
    // measures the air rather than the inside of an unrelated analyser.
    //
    // Mirrors ResolveGasRef in define_e2_set.f90. The two must agree, or this
    // interface names a humidity source the engine will not use.
    if (biometRhAvailable) { return biometMoistureRef; }

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
        //> The biomet sentinel is a valid reference, not a corrupt index, and
        //> survives. Everything else negative does not: this used to reset any
        //> negative value, which would now quietly turn "use the biomet" into
        //> "work it out yourself" on every load, and the dropdown would appear
        //> to forget what the user chose.
        if (gas.moistureRef < 0 && gas.moistureRef != biometMoistureRef)
        {
            gas.moistureRef = 0;
        }

        if (gas.cellRef > cells.size() || gas.cellRef < 0) { gas.cellRef = 0; }
    }
    validateTlagDonors(gases);
}

bool isTlagDonorFor(const QVector<GasRecord>& gases, int gasIndex,
                    int donorIndex)
{
    if (gasIndex < 0 || gasIndex >= gases.size()) { return false; }
    if (donorIndex < 0 || donorIndex >= gases.size()) { return false; }
    if (gasIndex == donorIndex) { return false; }

    const auto& gas = gases.at(gasIndex);
    const auto& donor = gases.at(donorIndex);
    //> Water on neither side: its lag is the one every other gas's water
    //> covariance is taken at, so the engine never moves it.
    if (gas.slug == kH2o || donor.slug == kH2o) { return false; }
    if (gas.rawColumn <= 0 || donor.rawColumn <= 0) { return false; }
    //> The same tube is the whole justification, and "other" names no tube.
    return isRealInstrument(gas.instrumentId)
        && donor.instrumentId == gas.instrumentId;
}

void validateTlagDonors(QVector<GasRecord>& gases)
{
    for (int i = 0; i < gases.size(); ++i)
    {
        //> Back to automatic rather than to some other record, for the same
        //> reason as moistureRef: automatic re-resolves on every run.
        const int donor = gases.at(i).tlagDonor;
        if (donor != 0 && !isTlagDonorFor(gases, i, donor - 1))
        {
            gases[i].tlagDonor = 0;
        }
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

QVector<CecPairRecord> defaultCecPairs(const QVector<GasRecord>& gases)
{
    QVector<CecPairRecord> pairs;
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).slug.compare(QStringLiteral("co2"), Qt::CaseInsensitive) != 0)
        { continue; }

        CecPairRecord pair;
        pair.meth = 1;
        pair.carbonIndex = i + 1;

        //> The water on the same analyser, because the octants are built from
        //> the water and the carbon together and across two instruments those
        //> two arrive at different time lags and through different spectral
        //> responses. Left at zero when that analyser carries none, which the
        //> engine resolves to the designated hygrometer and warns about.
        for (int j = 0; j < gases.size(); ++j)
        {
            if (gases.at(j).slug.compare(QStringLiteral("h2o"), Qt::CaseInsensitive) != 0)
            { continue; }
            if (gases.at(j).instrumentId != gases.at(i).instrumentId) { continue; }
            pair.waterIndex = j + 1;
            break;
        }
        pairs.append(pair);
    }
    return pairs;
}

namespace {

bool recordIs(const QVector<GasRecord>& gases, int recordIndex, const QString& slug)
{
    if (recordIndex < 1 || recordIndex > gases.size()) { return false; }
    return gases.at(recordIndex - 1).slug.compare(slug, Qt::CaseInsensitive) == 0;
}

/// The first measured record of a species, on \a instrumentId if one is given.
int firstRecordOf(const QVector<GasRecord>& gases, const QString& slug,
                  const QString& instrumentId = QString())
{
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).rawColumn <= 0) { continue; }
        if (gases.at(i).slug.compare(slug, Qt::CaseInsensitive) != 0) { continue; }
        if (!instrumentId.isNull() && gases.at(i).instrumentId != instrumentId) { continue; }
        return i + 1;
    }
    return 0;
}

QString instrumentOf(const QVector<GasRecord>& gases, int recordIndex)
{
    if (recordIndex < 1 || recordIndex > gases.size()) { return QString(); }
    const auto& id = gases.at(recordIndex - 1).instrumentId;
    return MeasurementRecords::isRealInstrument(id) ? id : QString();
}

/// Rows gathered into pairings with nothing dropped, and what was found on
/// the way: the one walk both cecPairsFromRows and cecRowsFit make, so the
/// table can never refuse what the file would have kept or keep what the
/// file would have dropped.
struct CecGroup
{
    CecPairRecord pair;
    int water = 0;
    bool octantReported = false;
    bool waterReported = false;
};

QVector<CecGroup> groupCecRows(const QVector<GasRecord>& gases,
                               const QVector<CecFluxRow>& rows,
                               bool* repeated)
{
    QVector<CecGroup> groups;
    if (repeated) { *repeated = false; }
    for (const auto& row : rows)
    {
        //> Grouped on the water the pairing READS, not on the cell's value:
        //> "(same analyser)" and that analyser's H2O named outright are one
        //> pairing, and the engine drops the second of two pairings that read
        //> the same pair of channels.
        const int water = MeasurementRecords::resolvedCecWater(
            gases, row.octantIndex, row.waterIndex);
        int at = -1;
        for (int g = 0; g < groups.size(); ++g)
        {
            if (groups.at(g).pair.carbonIndex == row.octantIndex
                && groups.at(g).water == water)
            {
                at = g;
                break;
            }
        }
        if (at < 0)
        {
            CecGroup group;
            group.pair.carbonIndex = row.octantIndex;
            group.pair.waterIndex = row.waterIndex;
            group.water = water;
            groups.append(group);
            at = groups.size() - 1;
        }

        auto& group = groups[at];
        bool again = false;
        if (row.octantIndex > 0 && row.fluxIndex == row.octantIndex)
        {
            again = group.octantReported;
            group.octantReported = true;
        }
        else if (row.fluxIndex == 0 || (water > 0 && row.fluxIndex == water))
        {
            again = group.waterReported;
            group.waterReported = true;
        }
        else
        {
            again = group.pair.extraIndices.contains(row.fluxIndex);
            if (!again) { group.pair.extraIndices.append(row.fluxIndex); }
        }
        if (again && repeated) { *repeated = true; }
    }

    for (auto& group : groups)
    {
        if (group.octantReported && group.waterReported) { group.pair.meth = 1; }
        else if (group.waterReported) { group.pair.meth = 2; }
        else if (group.octantReported) { group.pair.meth = 3; }
        else { group.pair.meth = MeasurementRecords::cecMethExtrasOnly; }
    }
    return groups;
}

} // namespace

bool isCecOctantGas(const QVector<GasRecord>& gases, int recordIndex)
{
    return recordIs(gases, recordIndex, QStringLiteral("co2"))
           || recordIs(gases, recordIndex, QStringLiteral("cos"));
}

bool isCecExperimental(const QVector<GasRecord>& gases, const CecFluxRow& row)
{
    if (recordIs(gases, row.octantIndex, QStringLiteral("cos"))) { return true; }
    //> The water row of a pairing whose water the engine resolves itself.
    if (row.fluxIndex == 0) { return false; }
    return !recordIs(gases, row.fluxIndex, QStringLiteral("co2"))
           && !recordIs(gases, row.fluxIndex, kH2o);
}

int resolvedCecWater(const QVector<GasRecord>& gases, int octantIndex, int waterIndex)
{
    if (waterIndex > 0) { return waterIndex; }
    const auto instrument = instrumentOf(gases, octantIndex);
    if (instrument.isNull()) { return 0; }
    return firstRecordOf(gases, kH2o, instrument);
}

int defaultCecWater(const QVector<GasRecord>& gases, int fluxIndex)
{
    if (recordIs(gases, fluxIndex, kH2o)) { return fluxIndex; }
    const auto instrument = instrumentOf(gases, fluxIndex);
    if (instrument.isNull()) { return 0; }
    return firstRecordOf(gases, kH2o, instrument);
}

int defaultCecOctant(const QVector<GasRecord>& gases, int fluxIndex)
{
    const auto co2 = QStringLiteral("co2");
    const auto cos = QStringLiteral("cos");
    if (recordIs(gases, fluxIndex, co2)) { return fluxIndex; }

    //> CO2 is the method as published, so it is what a row starts with
    //> wherever the site has one. Carbonyl sulfide octants are experimental
    //> and offered, never chosen for the user while a CO2 exists.
    const auto instrument = instrumentOf(gases, fluxIndex);
    if (!instrument.isNull())
    {
        if (const int on = firstRecordOf(gases, co2, instrument)) { return on; }
    }
    if (const int any = firstRecordOf(gases, co2)) { return any; }
    if (recordIs(gases, fluxIndex, cos)) { return fluxIndex; }
    if (!instrument.isNull())
    {
        if (const int on = firstRecordOf(gases, cos, instrument)) { return on; }
    }
    return firstRecordOf(gases, cos);
}

QVector<CecFluxRow> cecRowsFromPairs(const QVector<GasRecord>& gases,
                                     const QVector<CecPairRecord>& pairs)
{
    QVector<CecFluxRow> rows;
    for (const auto& pair : pairs)
    {
        if (pair.meth == 0) { continue; }
        if (pair.meth == 1 || pair.meth == 3)
        {
            rows.append({pair.carbonIndex, pair.waterIndex, pair.carbonIndex});
        }
        if (pair.meth == 1 || pair.meth == 2)
        {
            rows.append({resolvedCecWater(gases, pair.carbonIndex, pair.waterIndex),
                         pair.waterIndex, pair.carbonIndex});
        }
        for (const int extra : pair.extraIndices)
        {
            rows.append({extra, pair.waterIndex, pair.carbonIndex});
        }
    }
    return rows;
}

QVector<CecPairRecord> cecPairsFromRows(const QVector<GasRecord>& gases,
                                        const QVector<CecFluxRow>& rows)
{
    QVector<CecPairRecord> pairs;
    for (const auto& group : groupCecRows(gases, rows, nullptr))
    {
        if (pairs.size() >= cecMaxPairs) { break; }
        auto pair = group.pair;
        if (pair.extraIndices.size() > cecMaxExtras)
        {
            pair.extraIndices.resize(cecMaxExtras);
        }
        pairs.append(pair);
    }
    return pairs;
}

bool cecRowsFit(const QVector<GasRecord>& gases, const QVector<CecFluxRow>& rows,
                QString* reason)
{
    const auto say = [reason](const QString& text)
    {
        if (reason) { *reason = text; }
        return false;
    };

    for (const auto& row : rows)
    {
        if (row.octantIndex > 0 && !isCecOctantGas(gases, row.octantIndex))
        {
            return say(QCoreApplication::translate("MeasurementRecords",
                "Only a CO\xe2\x82\x82 or a COS channel can sort the octants."));
        }
    }

    bool repeated = false;
    const auto groups = groupCecRows(gases, rows, &repeated);
    if (repeated)
    {
        return say(QCoreApplication::translate("MeasurementRecords",
            "That flux is already partitioned with this water and these octants."));
    }
    if (groups.size() > cecMaxPairs)
    {
        return say(QCoreApplication::translate("MeasurementRecords",
            "The engine runs at most %1 combinations of octant gas and water. "
            "Give this row the octant gas and water of an existing row instead.")
                       .arg(cecMaxPairs));
    }
    for (const auto& group : groups)
    {
        if (group.pair.extraIndices.size() > cecMaxExtras)
        {
            return say(QCoreApplication::translate("MeasurementRecords",
                "One octant gas and water can carry at most %1 fluxes besides "
                "their own. Sort this one by a different octant gas or water.")
                           .arg(cecMaxExtras));
        }
    }
    if (reason) { reason->clear(); }
    return true;
}

} // namespace MeasurementRecords
