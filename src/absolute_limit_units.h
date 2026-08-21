/***************************************************************************
  absolute_limit_units.h
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

#ifndef ABSOLUTE_LIMIT_UNITS_H
#define ABSOLUTE_LIMIT_UNITS_H

#include <QString>

/// \file src/absolute_limit_units.h
/// \brief The unit an absolute limit is entered in, and what it is stored as.
///
/// The engine normalises every trace gas to umol/mol and water to mmol/mol
/// before the absolute-limits test runs (ConvertTraceGasUnits), so that is what
/// the stored limit means and it does not change here. What changes is the
/// number the user types: a gas reported in ppb could not be configured at all,
/// because ambient COS is 0.0005 umol/mol and the box carries three decimals.
namespace AbsoluteLimitUnits {

/// How a stored limit is shown, for one column.
struct Scale
{
    //> displayed = stored * factor. Always the reciprocal of the scaling the
    //> engine applies to the data, which is what keeps the two consistent:
    //> if it multiplies a ppb column by 1e-3 to reach umol/mol, a umol/mol
    //> limit is 1e3 times its value in ppb.
    double factor = 1.0;
    //> Suffix for the spin box.
    QString display;
    //> Whether the column states a mole fraction this can convert. False for a
    //> molar density, a voltage or an unstated unit, where the limit stays in
    //> the stored unit - the engine scales a density by StdVair at test time
    //> using a temperature and pressure it only knows at run time, so there is
    //> no factor to offer here.
    bool converted = false;
};

/// The scale for a column declared in \a unitToken, e.g. `ppb`.
///
/// \a unitToken is the ini token from the raw file description, never the
/// display string: this file's `ppt` means mmol/mol while `pmol_mol` is shown
/// as "pmol/mol (ppt)", so matching on what the user sees would swap them.
///
/// \a isWater selects the stored base, mmol/mol rather than umol/mol.
Scale forColumn(const QString& unitToken, bool isWater);

} // namespace AbsoluteLimitUnits

#endif // ABSOLUTE_LIMIT_UNITS_H
