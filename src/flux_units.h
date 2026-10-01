/***************************************************************************
  flux_units.h
  ------------
  Copyright © 2026-    , ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.
***************************************************************************/

#ifndef FLUX_UNITS_H
#define FLUX_UNITS_H

#include <QString>

/// \file src/flux_units.h
/// \brief The unit a per-gas flux threshold is entered in, and what it is
/// stored as.
///
/// Every trace gas flux is held internally, and compared against these
/// thresholds, in umol m-2 s-1 - the engine only rescales when it writes the
/// full output. So the stored threshold means umol m-2 s-1 whatever the gas,
/// and that does not change here. What changes is the number the user reads
/// and types: a COS flux is of the order of tens of nmol m-2 s-1, which is
/// 0.00001 in the stored unit, and a default of 0.01 umol m-2 s-1 filters out
/// every half-hour such a gas will ever produce.
namespace FluxUnits {

/// How a stored threshold is shown, for one gas column.
struct Scale
{
    //> displayed = stored * factor. The same scaling the engine applies to
    //> the flux before writing it, which is what keeps the two consistent: a
    //> threshold shown beside a nmol m-2 s-1 flux must itself be in nmol.
    double factor = 1.0;
    //> Suffix for the spin box.
    QString display;
    //> Whether the column states a unit this converts. False leaves the
    //> threshold in the stored unit, which is also what the engine reports
    //> for that column.
    bool converted = false;
};

/// The scale for a gas column declared in \a unitToken, e.g. `ppb`.
///
/// \a unitToken is the ini token from the raw file description, never the
/// display string: `ppt` means mmol/mol while `pmol_mol` is shown as
/// "pmol/mol (ppt)", so matching on what the user sees would swap them.
Scale forColumn(const QString& unitToken);

} // namespace FluxUnits

#endif // FLUX_UNITS_H
