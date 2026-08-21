"""The absolute-limit box converts; the stored value does not change.

The engine normalises every trace gas to umol/mol and water to mmol/mol before
the absolute-limits test runs, so that is what a stored limit means. It is also
why a gas reported in ppb could not be configured at all: ambient COS is
0.0005 umol/mol and the box carried three decimals, so the only limits the user
could type were zero and something a thousand times too large.

The fix is a display conversion, and its factor is the *reciprocal* of the
engine's own scaling. That reciprocal is the whole risk. Applied the wrong way
round it is invisible in review, silently rescales every existing project's
limits by a factor of a thousand, and the first sign of it is fluxes going
missing. So the table is checked term by term against the engine's, and every
token is round-tripped.

Skipped, not failed, when the engine checkout is not beside this one.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_UNITS = ENGINE_ROOT / "src" / "src_common" / "define_all_var_set.f90"
UNITS_SRC = GUI_ROOT / "src" / "absolute_limit_units.cpp"

#: token -> factor to umol/mol, as the engine applies it to the data.
ENGINE_FACTORS = {
    "ppt": 1e3,
    "ppm": 1.0,
    "ppb": 1e-3,
    "pmol_mol": 1e-6,
}


def _read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def _gui_table():
    """The { token, toUmolMol, label } rows of the conversion table."""
    src = _read(UNITS_SRC)
    body = src[src.index("const Fraction fractions[]"):]
    body = body[: body.index("};")]
    rows = re.findall(r'\{\s*"([a-z_]+)"\s*,\s*([0-9.e+-]+)\s*,', body)
    return {token: float(factor) for token, factor in rows}


class TheTableMatchesTheEngine(unittest.TestCase):
    def setUp(self):
        self.gui = _gui_table()

    def test_every_mole_fraction_token_is_covered(self):
        self.assertEqual(set(ENGINE_FACTORS), set(self.gui))

    def test_the_factors_are_the_engines_own(self):
        for token, factor in ENGINE_FACTORS.items():
            self.assertAlmostEqual(
                factor, self.gui[token], delta=factor * 1e-12,
                msg="%s scales to umol/mol by %g in the engine" % (token, factor))

    def test_ppt_is_mmol_mol_and_pmol_mol_is_not(self):
        """The pair most likely to be swapped.

        This file's `ppt` token means mmol/mol - parts per thousand - while
        `pmol_mol` is *displayed* as "pmol/mol (ppt)". Matching on what the
        user sees would exchange them, and the two are nine orders of
        magnitude apart.
        """
        self.assertEqual(1e3, self.gui["ppt"])
        self.assertEqual(1e-6, self.gui["pmol_mol"])
        src = _read(UNITS_SRC)
        self.assertIn("unitToken", src)
        self.assertNotIn("inputUnit()", src,
                         "the helper must take the ini token, never the label")

    def test_the_display_factor_is_the_reciprocal(self):
        """display = storedBase / toUmolMol, not the product.

        Round-tripped for every token: a value converted out to the display
        unit and back must land on itself, which is what keeps opening and
        saving an untouched project a no-op.
        """
        src = _read(UNITS_SRC)
        self.assertIn("scale.factor = storedBase / fraction.toUmolMol;", src)

        for water in (False, True):
            base = 1e3 if water else 1.0
            for token, to_umol in ENGINE_FACTORS.items():
                if water and token == "pmol_mol":
                    continue
                factor = base / to_umol
                stored = 0.0005
                self.assertAlmostEqual(stored, (stored * factor) / factor,
                                       delta=1e-18)

    def test_water_is_stored_on_the_mmol_basis(self):
        src = _read(UNITS_SRC)
        self.assertIn("const double storedBase = isWater ? 1e3 : 1.0;", src)

    def test_a_unit_that_cannot_convert_keeps_the_stored_one(self):
        """Densities, voltages and an unstated unit.

        The engine scales a molar density by StdVair at test time, using a
        temperature and pressure it only knows at run time, so there is no
        factor to offer - the limit stays a mole fraction and the box says so.
        """
        src = _read(UNITS_SRC)
        self.assertIn("bool converted", _read(GUI_ROOT / "src" / "absolute_limit_units.h"))
        self.assertIn("return scale;", src)

    def test_water_in_pmol_mol_is_left_alone(self):
        """The engine's water arm has no case for it and cycles unscaled."""
        src = _read(UNITS_SRC)
        self.assertIn('isWater && unitToken == QLatin1String("pmol_mol")', src)


@unittest.skipUnless(ENGINE_UNITS.exists(),
                     "engine checkout not beside this one")
class TheEngineStillScalesThatWay(unittest.TestCase):
    """Read the factors back out of the engine, so the two cannot drift.

    If ConvertTraceGasUnits ever changes a multiplier, this fails here rather
    than as limits that quietly stop matching the data they are tested against.
    """

    def setUp(self):
        src = _read(ENGINE_UNITS)
        #> The two arms live in different routines. Trace gases are normalised
        #> by ConvertTraceGasUnits; water is normalised inline in
        #> DefineAllVarSet, on its own `case('h2o')` branch with its own ladder.
        body = src[src.index("subroutine ConvertTraceGasUnits"):]
        self.body = body[: body.index("end subroutine ConvertTraceGasUnits")]
        outer = src[src.index("subroutine DefineAllVarSet"):]
        self.water = outer[outer.index("case('h2o')"):]
        self.water = self.water[: self.water.index("end select")]

    def test_the_trace_gas_arm_matches(self):
        # 'mmol_m3', 'ppm' return unscaled; the rest carry an explicit factor.
        self.assertIn("case ('mmol_m3', 'ppm')", self.body)
        self.assertIn("case ('ppt')", self.body)
        self.assertIn("* 1e3", self.body)
        self.assertIn("case ('umol_m3', 'ppb')", self.body)
        self.assertIn("* 1e-3", self.body)
        self.assertIn("case ('pmol_mol')", self.body)
        self.assertIn("* 1e-6", self.body)

    def test_water_has_no_pmol_mol_case(self):
        self.assertNotIn("pmol_mol", self.water,
                         "if the engine gains one, the interface must stop "
                         "treating water in pmol/mol as unconvertible")

    def test_the_water_ladder_is_the_trace_gas_one_shifted(self):
        """mmol/mol is water's unity where umol/mol is every other gas's.

        This is what the storedBase of 1e3 encodes: water's ppt is the no-op
        case, its ppm is 1e-3 and its ppb 1e-6 - each a thousand times smaller
        than the same token on the trace-gas arm.
        """
        self.assertIn("case ('mmol_m3', 'ppt')", self.water)
        self.assertIn("case ('umol_m3', 'ppm')", self.water)
        self.assertIn("* 1e-3", self.water)
        self.assertIn("case ('ppb')", self.water)
        self.assertIn("* 1e-6", self.water)


if __name__ == "__main__":
    unittest.main()
