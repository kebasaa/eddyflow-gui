/***************************************************************************
  absolute_limit_units.cpp
  ------------------------
  Copyright © 2026-    , ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.
***************************************************************************/

#include "absolute_limit_units.h"

#include "defs.h"

namespace AbsoluteLimitUnits {

Scale forColumn(const QString& unitToken, bool isWater)
{
    Scale scale;
    scale.display = isWater ? Defs::MMOL_MOL_STRING : Defs::UMOL_MOL_STRING;

    //> Mole fractions in descending order of magnitude, with the factor to
    //> umol/mol beside each - the engine's own scaling, inverted below.
    //>
    //>   ppt      mmol/mol   1e3
    //>   ppm      umol/mol   1
    //>   ppb      nmol/mol   1e-3
    //>   pmol_mol pmol/mol   1e-6
    struct Fraction { const char* token; double toUmolMol; const QString* label; };
    static const QString nmol = Defs::NMOL_MOL_STRING;
    static const QString pmol = QStringLiteral("pmol/mol");
    const Fraction fractions[] = {
        { "ppt",      1e3,  &Defs::MMOL_MOL_STRING },
        { "ppm",      1.0,  &Defs::UMOL_MOL_STRING },
        { "ppb",      1e-3, &nmol },
        { "pmol_mol", 1e-6, &pmol },
    };

    //> Water declared in pmol/mol is left alone. The engine's water arm has no
    //> case for that token and falls through unscaled, so its data reaches the
    //> test as though it were already mmol/mol; a factor here would describe a
    //> conversion that never happens.
    if (isWater && unitToken == QLatin1String("pmol_mol")) { return scale; }

    for (const auto& fraction : fractions)
    {
        if (unitToken != QLatin1String(fraction.token)) { continue; }

        //> Water is stored on the mmol basis, so its ladder is the same one
        //> shifted by a thousand: mmol/mol is water's unity where umol/mol is
        //> every other gas's.
        const double storedBase = isWater ? 1e3 : 1.0;
        scale.factor = storedBase / fraction.toUmolMol;
        scale.display = *fraction.label;
        scale.converted = true;
        return scale;
    }

    return scale;
}

} // namespace AbsoluteLimitUnits
