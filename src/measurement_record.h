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
};

/// The 19 per-gas processing settings that used to exist once per fixed slot.
struct GasProcessingSettings
{
    qreal srLim = -1.0;
    qreal dsHf = -1.0;
    qreal dsSf = -1.0;
    qreal alMin = -1.0;
    qreal alMax = -1.0;
    qreal tlDef = -1.0;
    qreal toMinFlux = -1.0;
    qreal toMinLag = -1.0;
    qreal toMaxLag = -1.0;
    qreal pwbMinLag = -1.0;
    qreal pwbMaxLag = -1.0;
    qreal saFmin = -1.0;
    qreal saFmax = -1.0;
    qreal saHfnFmin = -1.0;
    qreal saMinSt = -1.0;
    qreal saMinUn = -1.0;
    qreal saMax = -1.0;
    int outFullSp = 0;
    int outFullCospW = 0;
    int outRaw = 0;
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

    GasProcessingSettings proc;
};

namespace MeasurementRecords {

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

/// Index (1-based) of the H2O record that should correct \a gas.
///
/// Mirrors the engine's ResolveGasRef exactly: an explicit choice wins, then
/// the H2O on the same analyser, then the first H2O of any. Returns 0 when the
/// project has no H2O at all. Keep this and the engine in step - a difference
/// here shows up as fluxes that do not match what the interface promised.
int resolveMoistureRef(const QVector<GasRecord>& gases, int gasIndex);

/// Reset references that point at records which no longer exist.
void validateReferences(QVector<GasRecord>& gases,
                        const QVector<MeasurementRecord>& cells);

} // namespace MeasurementRecords

#endif // MEASUREMENT_RECORD_H
