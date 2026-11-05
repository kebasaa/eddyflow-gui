/***************************************************************************
  measurement_record.h
  --------------------
  Copyright © 2026-    , ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.
***************************************************************************/

#ifndef MEASUREMENT_RECORD_H
#define MEASUREMENT_RECORD_H

#include <QString>
#include <QVector>

/// \file src/measurement_record.h
/// \brief One measured column, named by species and instrument.
///
/// Replaces the single int per role (col_co2, col_h2o, ...) the project used
/// to carry. That shape allowed exactly one column per gas and gave no way to
/// say which analyser a column came from, so a site with two analysers could
/// not state that its CO2 should be corrected with its own H2O.

/// A non-gas measurement: cell temperature, cell pressure, a diagnostic.
struct MeasurementRecord
{
    /// Slug as the engine expects it: `co2`, `h2o`, `int_t_1`, `diag_72`, ...
    QString slug;
    /// Canonical instrument id (`li7200_1`), or the literals `other` / `none`.
    QString instrumentId;
    /// 1-based raw column; -1 when unset.
    int rawColumn = -1;

    bool isValid() const { return !slug.isEmpty() && rawColumn > 0; }

    //> Compared as a whole, so a derived list can be rebuilt and only stored
    //> when it actually differs - storing it unconditionally would mark the
    //> project modified on every metadata read.
    bool operator==(const MeasurementRecord& other) const
    {
        return slug == other.slug
               && instrumentId == other.instrumentId
               && rawColumn == other.rawColumn;
    }
    bool operator!=(const MeasurementRecord& other) const
        { return !(*this == other); }
};

/// The 20 per-gas processing settings that used to exist once per fixed slot.
struct GasProcessingSettings
{
    qreal srLim = -1.0;
    //> Consecutive-difference despiking: the largest step this gas may take
    //> from one sample to the next, in its OWN concentration unit. Not a
    //> sigma multiplier like srLim above - nothing scales it - so -1 means
    //> "not stated" and the engine leaves the column undespiked.
    qreal stepLim = -1.0;
    qreal dsHf = -1.0;
    qreal dsSf = -1.0;
    qreal alMin = -1.0;
    qreal alMax = -1.0;
    qreal tlDef = -1.0;
    qreal toMinFlux = -1.0;
    qreal toMinLag = -1.0;
    qreal toMaxLag = -1.0;
    //> Not the -1.0 the neighbours use. A PWB search window may legitimately
    //> open at a negative lag, and -1.0 s is a perfectly ordinary bound - so
    //> with that sentinel a user could neither state -1.0 nor state 0.0 (the
    //> dialog showed 0.0 for "unset", and setting a spin to the value it
    //> already displays emits nothing). Both are expressible against this one.
    qreal pwbMinLag = -9999.0;
    qreal pwbMaxLag = -9999.0;
    qreal saFmin = -1.0;
    qreal saFmax = -1.0;
    qreal saHfnFmin = -1.0;
    qreal saMinSt = -1.0;
    qreal saMinUn = -1.0;
    qreal saMax = -1.0;
    //> The months this gas pools before a transfer function is fitted, as a
    //> group list: `1-12` is one group over the calendar, `1-6,7-12` is two,
    //> and a group's ordinal in the list is its class index.
    //>
    //> **Empty means the record carries no decision**, the same convention as
    //> the -1 above and for the same reason: the engine applies a per-gas
    //> override whenever the tag is present, and its own default is one group
    //> spanning the calendar. This replaces three flat tables - sa_co2_g*,
    //> sa_ch4_g* and sa_gas4_g*, twelve start/stop pairs each - which is why
    //> every gas past the fourth used to inherit CO2's grouping.
    QString saMonths;
    //> Output selections. **-1 means the record carries no decision**, which
    //> is not the same as 0: the engine applies a per-gas override whenever
    //> the tag is present, so an undecided flag must be left out of the file
    //> rather than written as off, and the interface must fall back to the
    //> legacy flat key rather than showing the box unchecked.
    int outFullSp = -1;
    int outFullCospW = -1;
    int outRaw = -1;
};

/// A measured gas.
struct GasRecord : MeasurementRecord
{
    /// 1-based index into the gas record list naming the H2O this gas is
    /// corrected with. **0 means auto**, which is what keeps old projects and
    /// untouched selections self-correcting: an automatic reference resolves
    /// afresh every time and can never go stale.
    int moistureRef = 0;
    /// 1-based index into the cell record list; 0 means auto.
    int cellRef = 0;
    /// Molecular weight and diffusivity overrides. -1 means "use the
    /// GasMetadata default for this slug", and nothing is written out.
    qreal mw = -1.0;
    qreal diff = -1.0;
    /// Eugster and Senn (1995) damping time constant L [s]. -1 means "use
    /// the analyser's response time", and nothing is written out. On the
    /// record rather than in proc because it is a [Project] key: RP applies
    /// that correction itself.
    qreal esTau = -1.0;
    /// 1-based index into the gas record list naming the tube-mate this gas
    /// borrows its time lag from, when lag borrowing is chosen per gas.
    /// **0 means automatic**: the best-resolved gas on the analyser. Kept on
    /// the record because it is an index into this list, and every
    /// rearrangement of the list has to carry it the way it carries
    /// moistureRef. Written to [RawProcess_Settings] as tlag_borrow_pairs,
    /// not to [Project].
    int tlagDonor = 0;

    GasProcessingSettings proc;
};

//> One Conditional Eddy Covariance pairing: an octant gas, a water channel,
//> and any further species partitioned in the octants those two define.
//> Indices are 1-based into the gas record list, matching the engine's
//> cec_<i>_co2 / cec_<i>_h2o keys; 0 means "not stated", which the engine
//> resolves from the analyser layout.
//>
//> carbonIndex is the OCTANT GAS: a CO2 record, as Zahn et al. (2022) define
//> the method, or - experimentally, from project format 5.2.0 - a carbonyl
//> sulfide record. The engine replaces anything else with the designated CO2
//> and says so (Warning 123).
//>
//> This is what the project file stores. The CEC Settings table shows it one
//> flux per row instead (CecFluxRow), and converts between the two with
//> cecRowsFromPairs / cecPairsFromRows.
struct CecPairRecord
{
    //> 0 off, 1 water and octant gas, 2 water only, 3 octant gas only,
    //> 4 extras only.
    int meth = 1;
    int carbonIndex = 0;
    int waterIndex = 0;
    QVector<int> extraIndices;

    bool operator==(const CecPairRecord& other) const
    {
        return meth == other.meth
               && carbonIndex == other.carbonIndex
               && waterIndex == other.waterIndex
               && extraIndices == other.extraIndices;
    }
    bool operator!=(const CecPairRecord& other) const { return !(*this == other); }
};

//> One row of the CEC Settings table: one flux to partition, the water whose
//> moist ejections select the parcels, and the octant gas whose sign sorts
//> them into stomatal and non-stomatal. 1-based record indices like the
//> pairing's. waterIndex 0 is "the water on the octant gas's analyser", as in
//> CecPairRecord; fluxIndex 0 is only ever the water row of a pairing whose
//> water the engine resolves itself, to the designated hygrometer.
//>
//> Never stored. Rows sharing an octant gas and a water are one pairing, and
//> that pairing is what the project file carries.
struct CecFluxRow
{
    int fluxIndex = 0;
    int waterIndex = 0;
    int octantIndex = 0;

    bool operator==(const CecFluxRow& other) const
    {
        return fluxIndex == other.fluxIndex
               && waterIndex == other.waterIndex
               && octantIndex == other.octantIndex;
    }
    bool operator!=(const CecFluxRow& other) const { return !(*this == other); }
};

namespace MeasurementRecords {

/// The engine's MaxNumCecPairs and MaxNumCecExtra (m_typedef.f90). A project
/// stating more is truncated by the engine without a word, so the table
/// refuses to build one.
inline constexpr int cecMaxPairs = 8;
inline constexpr int cecMaxExtras = 4;
/// cec_<i>_meth for a pairing that reports neither of its own two gases, only
/// the extras sorted by them.
inline constexpr int cecMethExtrasOnly = 4;

/// `moistureRef` value meaning "the biomet relative humidity".
///
/// Negative so it can never be mistaken for a 1-based record index, and the
/// same number as the engine's `biometMoistRef` in m_typedef.f90. The two are
/// one value written down twice, with the project file carrying it between
/// them; changing one alone silently repoints every gas that names the biomet
/// at a hygrometer instead.
inline constexpr int biometMoistureRef = -1;

/// Instrument id used when a variable has no analyser (legal for cell T/P).
inline QString noneInstrument() { return QStringLiteral("none"); }
/// Instrument id used when the user picked "Other".
inline QString otherInstrument() { return QStringLiteral("other"); }

/// Whether \a instrumentId identifies a real device.
///
/// `other` and `none` are excluded: many unrelated variables carry "Other",
/// so matching on it would pair a gas with an arbitrary H2O.
inline bool isRealInstrument(const QString& instrumentId)
{
    return !instrumentId.isEmpty()
        && instrumentId != noneInstrument()
        && instrumentId != otherInstrument();
}

/// The moisture source this gas will actually be corrected with.
///
/// Mirrors the engine's ResolveGasRef: an explicit choice wins, then the H2O
/// on the gas's own analyser, then the biomet. Returns biometMoistureRef for
/// the biomet, a 1-based record index for a hygrometer, and 0 for nothing.
/// Keep this and the engine in step - a difference here shows up as fluxes
/// that do not match what the interface promised.
///
/// The old last rule, "the first H2O of any analyser", is gone from both:
/// it handed a gas whose own analyser has no hygrometer another instrument's
/// water, through a different cell at a different time lag.
///
/// biometRhAvailable says whether the project names a biomet RH column;
/// neither of the last two rules can be answered without it.
int resolveMoistureRef(const QVector<GasRecord>& gases, int gasIndex,
                       bool biometRhAvailable = false);

/// Reset references that point at records which no longer exist.
void validateReferences(QVector<GasRecord>& gases,
                        const QVector<MeasurementRecord>& cells);

/// Whether record \a donorIndex (0-based) may lend its time lag to record
/// \a gasIndex: a different, measured, non-water gas on the same real
/// analyser.
///
/// Mirrors the per-gas branch of BorrowTimelagBelowDetectionLimit in the
/// engine's borrow_timelag.f90, which also refuses open-path gases - this
/// interface does not know a path type, so the engine has the last word there.
bool isTlagDonorFor(const QVector<GasRecord>& gases, int gasIndex,
                    int donorIndex);

/// Clear every tlagDonor that is no longer a tube-mate of its gas.
void validateTlagDonors(QVector<GasRecord>& gases);

/// Row label: the species, qualified by the analyser when there is one, so
/// two CO2 records on different analysers are told apart.
QString gasLabel(const QVector<GasRecord>& gases, int index);

/// The configured records, in the order they should be shown: alphabetical
/// by gasLabel().
///
/// Returns record indices, not positions - record order is the engine's slot
/// mapping and the key of every gas_<N>_* setting, so it is sorted for reading
/// only and never in the list itself. Whatever a caller builds from this has
/// to keep carrying the index, or a widget ends up writing to another gas.
///
/// Records with no raw column are left out: they are slots kept so the
/// mapping stays put, not measurements.
QVector<int> gasDisplayOrder(const QVector<GasRecord>& gases);

//> The pairings a site means when it has not said: one per carbon channel,
//> each with the water on the same analyser.
//>
//> Mirrors AutoCecPairs in the engine's gas_slot_resolution.f90, and must keep
//> mirroring it - the engine derives the same list when the project states
//> none, so a difference here is a project whose interface and whose run
//> disagree about which channels were paired. Same contract as
//> resolveMoistureRef above.
QVector<CecPairRecord> defaultCecPairs(const QVector<GasRecord>& gases);

/// Whether a record can sort the octants: CO2, or carbonyl sulfide
/// (experimental). The same two species the engine's CecPairs accepts.
bool isCecOctantGas(const QVector<GasRecord>& gases, int recordIndex);

/// Whether a row goes beyond Zahn et al. (2022): a flux that is neither CO2
/// nor water, or octants sorted by carbonyl sulfide. Shown as experimental.
bool isCecExperimental(const QVector<GasRecord>& gases, const CecFluxRow& row);

/// The water a pairing actually reads: the stated one, else the H2O on the
/// octant gas's analyser, else 0 - the engine's designated hygrometer, which
/// only the engine knows. Mirrors CecWaterOnAnalyserOf as far as the project
/// can.
int resolvedCecWater(const QVector<GasRecord>& gases, int octantIndex,
                     int waterIndex);

/// What a row partitioning \a fluxIndex starts with: the water and the octant
/// gas on the flux's own analyser. A water flux is its own water and a CO2
/// flux its own octant gas; without a CO2 on the analyser the first CO2
/// anywhere, and only failing every CO2 a carbonyl sulfide.
int defaultCecWater(const QVector<GasRecord>& gases, int fluxIndex);
int defaultCecOctant(const QVector<GasRecord>& gases, int fluxIndex);

/// One row per flux the pairings report: the octant gas (meth 1 or 3), the
/// water (meth 1 or 2), then the extras. A pairing that is off reports
/// nothing and contributes no row.
QVector<CecFluxRow> cecRowsFromPairs(const QVector<GasRecord>& gases,
                                     const QVector<CecPairRecord>& pairs);

/// The pairings the rows mean: one per (octant gas, water actually read), in
/// order of first appearance. A row whose flux is the octant gas or that water
/// switches that gas on; every other flux is an extra. Repeats are dropped,
/// and so is anything past the engine's limits - check cecRowsFit first.
QVector<CecPairRecord> cecPairsFromRows(const QVector<GasRecord>& gases,
                                        const QVector<CecFluxRow>& rows);

/// Whether the rows fit what the engine holds, and if not, why not.
/// \a reason is set for the first problem found.
bool cecRowsFit(const QVector<GasRecord>& gases, const QVector<CecFluxRow>& rows,
                QString* reason = nullptr);

} // namespace MeasurementRecords

#endif // MEASUREMENT_RECORD_H
