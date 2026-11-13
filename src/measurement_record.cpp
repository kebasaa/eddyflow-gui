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
    validateSaSources(gases);
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

bool isSaSourceFor(const QVector<GasRecord>& gases, int gasIndex,
                   int sourceIndex)
{
    if (gasIndex < 0 || gasIndex >= gases.size()) { return false; }
    if (sourceIndex < 0 || sourceIndex >= gases.size()) { return false; }
    if (gasIndex == sourceIndex) { return false; }

    const auto& gas = gases.at(gasIndex);
    const auto& source = gases.at(sourceIndex);
    //> Water on neither side: its cut-off depends on humidity through wall
    //> sorption, which no passive gas has, and the engine reads it from an
    //> RH regression rather than from a month class.
    if (gas.slug == kH2o || source.slug == kH2o) { return false; }
    if (gas.rawColumn <= 0 || source.rawColumn <= 0) { return false; }
    //> The fit measures the analyser's tube, cell and response; another
    //> analyser has others, and "other" names none.
    return isRealInstrument(gas.instrumentId)
        && source.instrumentId == gas.instrumentId;
}

void validateSaSources(QVector<GasRecord>& gases)
{
    for (int i = 0; i < gases.size(); ++i)
    {
        //> Back to automatic, which re-resolves on every run, rather than to
        //> some other record. "Own" names no record and stays.
        const int source = gases.at(i).saSource;
        if (source > 0 && !isSaSourceFor(gases, i, source - 1))
        {
            gases[i].saSource = 0;
        }
        else if (source < 0 && source != kSaSourceOwn)
        {
            gases[i].saSource = 0;
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

/// \a repeatedRow, when given, is set to the 0-based row that already held
/// the flux a later row repeats, and \a repeatingRow to that later row; both
/// -1 when nothing repeats.
///
/// The groups come back in a canonical order - by octant record, then by the
/// water actually read - and not in order of first appearance. The table
/// sorts its rows by flux, so the order rows arrive in says nothing about
/// the pairings; were it to decide the file's order, a round trip through a
/// sorted table could renumber them, and with them every _1/_2 suffix.
QVector<CecGroup> groupCecRows(const QVector<GasRecord>& gases,
                               const QVector<CecFluxRow>& rows,
                               bool* repeated,
                               int* repeatedRow = nullptr,
                               int* repeatingRow = nullptr)
{
    QVector<CecGroup> groups;
    //> Which row set each target, so a repeat can name the row it repeats.
    QVector<QVector<QPair<int, int>>> setBy;
    if (repeated) { *repeated = false; }
    if (repeatedRow) { *repeatedRow = -1; }
    if (repeatingRow) { *repeatingRow = -1; }
    for (int r = 0; r < rows.size(); ++r)
    {
        const auto& row = rows.at(r);
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
            setBy.append(QVector<QPair<int, int>>());
            at = groups.size() - 1;
        }

        auto& group = groups[at];
        //> The target this row sets: -1 the octant gas, -2 the water, else
        //> the extra's own record.
        int target = row.fluxIndex;
        bool again = false;
        if (row.octantIndex > 0 && row.fluxIndex == row.octantIndex)
        {
            target = -1;
            again = group.octantReported;
            group.octantReported = true;
        }
        else if (row.fluxIndex == 0 || (water > 0 && row.fluxIndex == water))
        {
            target = -2;
            again = group.waterReported;
            group.waterReported = true;
        }
        else
        {
            again = group.pair.extraIndices.contains(row.fluxIndex);
            if (!again) { group.pair.extraIndices.append(row.fluxIndex); }
        }
        if (again)
        {
            if (repeated && !*repeated)
            {
                *repeated = true;
                for (const auto& seen : setBy.at(at))
                {
                    if (seen.first != target) { continue; }
                    if (repeatedRow) { *repeatedRow = seen.second; }
                    break;
                }
                if (repeatingRow) { *repeatingRow = r; }
            }
        }
        else
        {
            setBy[at].append(qMakePair(target, r));
        }
    }

    for (auto& group : groups)
    {
        if (group.octantReported && group.waterReported) { group.pair.meth = 1; }
        else if (group.waterReported) { group.pair.meth = 2; }
        else if (group.octantReported) { group.pair.meth = 3; }
        else { group.pair.meth = MeasurementRecords::cecMethExtrasOnly; }
    }
    std::stable_sort(groups.begin(), groups.end(),
                     [](const CecGroup& left, const CecGroup& right)
                     {
                         if (left.pair.carbonIndex != right.pair.carbonIndex)
                         {
                             return left.pair.carbonIndex < right.pair.carbonIndex;
                         }
                         return left.water < right.water;
                     });
    return groups;
}

/// Where each row's pairing lands among the canonically ordered groups.
QVector<int> groupIndexOfRows(const QVector<GasRecord>& gases,
                              const QVector<CecFluxRow>& rows,
                              const QVector<CecGroup>& groups)
{
    QVector<int> out;
    out.reserve(rows.size());
    for (const auto& row : rows)
    {
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
        out.append(at);
    }
    return out;
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
    int repeatedRow = -1;
    int repeatingRow = -1;
    const auto groups = groupCecRows(gases, rows, &repeated, &repeatedRow, &repeatingRow);
    if (repeated)
    {
        const int flux = repeatingRow >= 0 ? rows.at(repeatingRow).fluxIndex : 0;
        const auto name = flux > 0
                              ? gasLabel(gases, flux - 1)
                              : QCoreApplication::translate("MeasurementRecords", "That flux");
        if (repeatedRow >= 0)
        {
            return say(QCoreApplication::translate("MeasurementRecords",
                "%1 is already split with this water and these octants, in row %2. "
                "Choose another water or octant gas to split it a second way.")
                           .arg(name).arg(repeatedRow + 1));
        }
        return say(QCoreApplication::translate("MeasurementRecords",
            "%1 is already split with this water and these octants.").arg(name));
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

QVector<int> cecRowPairIndices(const QVector<GasRecord>& gases,
                               const QVector<CecFluxRow>& rows)
{
    const auto groups = groupCecRows(gases, rows, nullptr);
    auto out = groupIndexOfRows(gases, rows, groups);
    //> Past the engine's limit a group is not written, so it has no pairing.
    for (auto& at : out)
    {
        if (at >= cecMaxPairs) { at = -1; }
    }
    return out;
}

QVector<CecFluxRow> sortCecRowsForDisplay(const QVector<GasRecord>& gases,
                                          const QVector<CecFluxRow>& rows)
{
    //> Alphabetically, as the cells read: by flux, so every version of one
    //> gas sits together, then by octant gas, then by the water actually
    //> read. Every key comes from what the row means rather than from where
    //> it was added, so the order is the same however the rows arrived -
    //> which is what lets the table re-sort after every edit without
    //> breaking "what you see is what reloads". Record order breaks a tie,
    //> which only two channels with one label can produce.
    const auto labelOf = [&gases](int recordIndex)
    {
        if (recordIndex < 1 || recordIndex > gases.size()) { return QString(); }
        return gasLabel(gases, recordIndex - 1);
    };
    const auto compare = [&labelOf](int left, int right)
    {
        //> An unresolved channel - the designated hygrometer or CO2, which
        //> only the engine knows - after every named one.
        if ((left > 0) != (right > 0)) { return left > 0 ? -1 : 1; }
        const int byLabel = QString::compare(labelOf(left), labelOf(right),
                                             Qt::CaseInsensitive);
        if (byLabel != 0) { return byLabel; }
        return left < right ? -1 : (left > right ? 1 : 0);
    };

    QVector<int> at(rows.size());
    for (int i = 0; i < rows.size(); ++i) { at[i] = i; }
    std::stable_sort(at.begin(), at.end(), [&](int left, int right)
    {
        const auto& l = rows.at(left);
        const auto& r = rows.at(right);
        if (const int c = compare(l.fluxIndex, r.fluxIndex)) { return c < 0; }
        if (const int c = compare(l.octantIndex, r.octantIndex)) { return c < 0; }
        const int c = compare(resolvedCecWater(gases, l.octantIndex, l.waterIndex),
                              resolvedCecWater(gases, r.octantIndex, r.waterIndex));
        return c < 0;
    });

    QVector<CecFluxRow> out;
    out.reserve(rows.size());
    for (const int i : at) { out.append(rows.at(i)); }
    return out;
}

QStringList cecPairSuffixes(const QVector<GasRecord>& gases,
                            const QVector<CecPairRecord>& pairs)
{
    //> The engine's rule, CecPairs in gas_slot_resolution.f90, step for step:
    //> the octant gas's analyser, or pair<n> when it has no name; its species
    //> when it is not CO2; where two pairings share that, a water on another
    //> named analyser; and a number for whatever is still shared. Two things
    //> only the engine resolves are left out - the designated CO2 behind an
    //> octant gas of 0 and the designated hygrometer behind a water it cannot
    //> find on the analyser - so a project relying on those may be named
    //> differently in the run.
    const auto lowerInstrument = [&gases](int recordIndex)
    {
        return instrumentOf(gases, recordIndex).toLower();
    };

    QStringList bases;
    for (int i = 0; i < pairs.size(); ++i)
    {
        const auto& pair = pairs.at(i);
        auto base = lowerInstrument(pair.carbonIndex);
        if (base.isEmpty()) { base = QStringLiteral("pair%1").arg(i + 1); }
        if (pair.carbonIndex >= 1 && pair.carbonIndex <= gases.size())
        {
            const auto species = gases.at(pair.carbonIndex - 1).slug.toLower();
            if (!species.isEmpty() && species != QStringLiteral("co2"))
            {
                base += QLatin1Char('_') + species;
            }
        }
        bases.append(base);
    }

    QStringList names;
    for (int i = 0; i < pairs.size(); ++i)
    {
        auto name = bases.at(i);
        if (bases.count(bases.at(i)) > 1)
        {
            const auto& pair = pairs.at(i);
            const auto water = lowerInstrument(
                resolvedCecWater(gases, pair.carbonIndex, pair.waterIndex));
            if (!water.isEmpty() && water != lowerInstrument(pair.carbonIndex))
            {
                name += QStringLiteral("_h2o_") + water;
            }
        }
        names.append(name);
    }

    QStringList out;
    for (int i = 0; i < names.size(); ++i)
    {
        if (names.count(names.at(i)) == 1)
        {
            out.append(QLatin1Char('_') + names.at(i));
            continue;
        }
        int occurrence = 0;
        for (int k = 0; k <= i; ++k)
        {
            if (names.at(k) == names.at(i)) { ++occurrence; }
        }
        out.append(QStringLiteral("_%1_%2").arg(names.at(i)).arg(occurrence));
    }
    return out;
}

bool cecNextFreeVariant(const QVector<GasRecord>& gases,
                        const QVector<CecFluxRow>& rows, int fluxIndex,
                        int preferredWater, int preferredOctant, CecFluxRow* found)
{
    //> Channels on the flux's own analyser before those on another, and
    //> within each the published method before the experiment: CO2, then COS.
    //> So a second version of the MIRO's COS is sorted by the MIRO's COS, not
    //> by another analyser's CO2 that would only add a time-lag mismatch.
    const auto own = instrumentOf(gases, fluxIndex);
    const auto add = [&gases, &own](QVector<int>& into, const QString& slug, bool onOwn)
    {
        for (int i = 0; i < gases.size(); ++i)
        {
            if (gases.at(i).rawColumn <= 0) { continue; }
            if (gases.at(i).slug.compare(slug, Qt::CaseInsensitive) != 0) { continue; }
            const bool same = !own.isNull() && gases.at(i).instrumentId == own;
            if (same != onOwn) { continue; }
            if (!into.contains(i + 1)) { into.append(i + 1); }
        }
    };
    const auto co2 = QStringLiteral("co2");
    const auto cos = QStringLiteral("cos");
    QVector<int> octants{preferredOctant};
    add(octants, co2, true);
    add(octants, cos, true);
    add(octants, co2, false);
    add(octants, cos, false);
    QVector<int> waters{preferredWater};
    add(waters, kH2o, true);
    add(waters, kH2o, false);

    //> Another octant gas before another water: splitting one gas by CO2 and
    //> by COS octants is the comparison this exists for.
    for (const int water : waters)
    {
        for (const int octant : octants)
        {
            if (octant <= 0) { continue; }
            CecFluxRow row;
            row.fluxIndex = fluxIndex;
            row.waterIndex = water;
            row.octantIndex = octant;
            auto candidate = rows;
            candidate.append(row);
            if (cecRowsFit(gases, candidate))
            {
                if (found) { *found = row; }
                return true;
            }
        }
    }
    return false;
}

} // namespace MeasurementRecords
