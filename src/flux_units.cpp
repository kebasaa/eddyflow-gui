/***************************************************************************
  flux_units.cpp
  --------------
  Copyright © 2026-    , ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.
***************************************************************************/

#include "flux_units.h"

#include "defs.h"

namespace FluxUnits {

Scale forColumn(const QString& unitToken)
{
    Scale scale;
    scale.display = Defs::UMOL_M2S_STRING;

    //> The engine's own table, in GasFullOutputUnits: a gas whose column is
    //> declared in ppb is reported in nmol m-2 s-1, one in pmol/mol in pmol
    //> m-2 s-1, and everything else - mole fractions on larger bases, molar
    //> densities, mass densities - in umol m-2 s-1, the unit the flux is
    //> held in. Keeping the two tables the same is the point: the threshold
    //> is read in whatever unit the flux beside it is reported in.
    struct Basis { const char* token; double factor; const QString* label; };
    const Basis bases[] = {
        { "ppb",      1e3, &Defs::NMOL_M2S_STRING },
        { "nmol_mol", 1e3, &Defs::NMOL_M2S_STRING },
        { "pmol_mol", 1e6, &Defs::PMOL_M2S_STRING },
    };

    for (const auto& basis : bases)
    {
        if (unitToken != QLatin1String(basis.token)) { continue; }

        scale.factor = basis.factor;
        scale.display = *basis.label;
        scale.converted = true;
        return scale;
    }

    return scale;
}

} // namespace FluxUnits
