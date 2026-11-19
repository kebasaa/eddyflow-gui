/***************************************************************************
  spectralassessmentfile.h
  ------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef SPECTRALASSESSMENTFILE_H
#define SPECTRALASSESSMENTFILE_H

#include <QHash>
#include <QString>
#include <QVector>

struct GasRecord;

/// The per-gas transfer-function fits a spectral assessment file holds, and
/// the engine's rule for when one is implausible.
///
/// Read-only and for display: the engine reads the file itself. The interface
/// reads it to show each gas's own fit where its source is chosen, and to
/// preview which gases the engine would hand a tube-mate's fit under automatic
/// selection.
///
/// Blocks are matched to records the engine's way: by the `var=`/`instr=`
/// stamp the header carries, then by the block name, never by position. The
/// file validator (AncillaryFileTest) keeps its own reader, which the static
/// tests pin in place.
namespace SpectralAssessmentFile {

/// The engine's error code, which the file writes for a month not fitted.
constexpr double kError = -9999.0;

/// The plausibility thresholds. They must equal SaFnMin and SaFcRatioMin in
/// the engine's m_common_global_var.f90; tests/test_sa_source_contract_static.py
/// holds them together.
///
/// Fn is the fitted low-frequency level of the gas/temperature spectral
/// ratio: far below 1, the ratio is wrong where the fit normalises, which is
/// noise or drift rather than attenuation. And gases on one analyser share its
/// tube, cell and response, so their cut-offs should agree; more than four
/// times apart is not attenuation either.
constexpr double kFnMin = 0.2;
constexpr double kFcRatioMin = 0.25;

/// One month's fit.
struct Fit
{
    double fn = kError;
    double fc = kError;
};

/// Twelve months, January first, per 0-based gas record. A record with no
/// block in the file has no entry.
using Fits = QHash<int, QVector<Fit>>;

/// The month blocks of \a path, a local file. Empty when it cannot be read or
/// holds no month block. Water's RH-class blocks are skipped: water never
/// lends or borrows a transfer function.
Fits readFits(const QVector<GasRecord>& gases, const QString& path);

/// A fit that can stand as a transfer function: a positive cut-off and an Fn
/// of at least kFnMin.
bool fitUsable(const Fit& fit);

/// The tube-mate record \a gasIndex should take its transfer function from in
/// \a month (0-11), or -1 when its own fit is plausible or nothing better is
/// on its analyser. The mirror of SpectralSuggestedSource in the engine's
/// gas_slot_resolution.f90.
int suggestedSource(const QVector<GasRecord>& gases, const Fits& fits,
                    int gasIndex, int month);

/// The source suggested for the most months, or -1 for none.
int suggestedSourceOverall(const QVector<GasRecord>& gases, const Fits& fits,
                           int gasIndex);

/// "Fn 0.036, fc 0.00199 Hz", or ranges when the months differ; "not fitted"
/// when no month has a cut-off.
QString fitSummary(const QVector<Fit>& months);

} // namespace SpectralAssessmentFile

#endif // SPECTRALASSESSMENTFILE_H
