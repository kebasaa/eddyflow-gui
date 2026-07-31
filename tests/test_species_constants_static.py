"""The two species tables must agree, or the same gas gets two constants.

The interface holds molecular weight and diffusivity for every gas it offers
(GasMetadata's registry); the engine holds its own copy, consulted whenever a
gas record carries no value of its own. Nothing at build time connects the two
- they are separate repositories - so a value corrected on one side and not
the other yields a flux that is silently wrong by exactly that ratio.

The engine's table used to know four species. Everything else - COS, CO, SO2,
NH3, O3, NO2, NO, N2, O2, Ar, all of which the interface offers - fell to a
`case default` holding nitrous oxide's 44.01 g/mol. Measured on base_mw with a
mass-basis COS column, that put the flux out by 60.075/44.01, a factor of
1.365.

Units differ by construction and are converted here rather than in either
source: the interface stores g/mol and cm2 s-1 because that is what its
spin boxes show, the engine stores kg/mol and m2 s-1 because that is what the
physics uses.

Skipped, not failed, when the engine checkout is not beside this one.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_SRC = ENGINE_ROOT / "src" / "src_common" / "write_processing_project_variables.f90"
GAS_METADATA = GUI_ROOT / "src" / "gas_metadata.cpp"

#> The species the interface will actually let a user select as a gas:
#> CO2/H2O/CH4 plus the registry entries whose diffusivity is not Manual.
#> Those are the only ones that can reach the engine's table from the GUI.
SELECTABLE = [
    "co2", "h2o", "ch4", "n2o", "co", "so2", "nh3",
    "o3", "no2", "no", "n2", "o2", "ar", "cos",
]


def _read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def _engine_table(function_name):
    """Species -> value, from one of the engine's select-case tables."""
    src = _read(ENGINE_SRC)
    start = src.index("function %s(var)" % function_name)
    end = src.index("end function %s" % function_name)
    body = src[start:end]
    table = {}
    for species, value in re.findall(
        r"case\s*\('([A-Z0-9]+)'\);\s*%s\s*=\s*([0-9.eEdD+-]+)" % function_name, body
    ):
        table[species.lower()] = float(value.replace("d", "e").replace("D", "e"))
    return table


def _gui_registry():
    """Species -> (molecular weight [g/mol], diffusivity [cm2/s]).

    The registry writes formulas with Unicode subscripts via Defs::SUBTWO and
    friends, so the name is reassembled from the parts rather than read as a
    literal.
    """
    src = _read(GAS_METADATA)
    start = src.index("static const QVector<GasEntry> registry")
    end = src.index("return registry;")
    body = src[start:end]

    subs = {"Defs::SUBTWO": "2", "Defs::SUBTHREE": "3", "Defs::SUBFOUR": "4"}
    table = {}
    for line in body.splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        m = re.match(r"\{(.+?),\s*([0-9.]+),\s*([0-9.]+),", line)
        if not m:
            continue
        name_expr, mw, diff = m.groups()
        name = ""
        for part in name_expr.split("+"):
            part = part.strip()
            if part in subs:
                name += subs[part]
                continue
            #> Subscripts past four are written as raw code points, e.g.
            #> QChar(0x2085) for the 5 in N2O5. Dropping them silently made
            #> N2O5 collide with N2O and overwrite its constants.
            qchar = re.match(r"QChar\(0x(20[89][0-9a-fA-F])\)", part)
            if qchar:
                name += str(int(qchar.group(1), 16) - 0x2080)
                continue
            lit = re.match(r"Q(?:StringLiteral|Latin1Char)\(['\"](.*)['\"]\)", part)
            if lit:
                name += lit.group(1)
        if name:
            table[name.lower()] = (float(mw), float(diff))
    return table


@unittest.skipUnless(ENGINE_SRC.exists(), "engine checkout not beside this one")
class TheSpeciesTablesAgree(unittest.TestCase):
    #> The interface's three historic gases live in findSpecies' own small
    #> table rather than in the registry, so they are stated here.
    HISTORIC = {
        "co2": (44.01, 0.1381),
        "h2o": (18.02, 0.2178),
        "ch4": (16.04, 0.1952),
    }

    def setUp(self):
        self.mw = _engine_table("DefaultMolecularWeight")
        self.dc = _engine_table("DefaultDiffusivity")
        self.gui = _gui_registry()
        self.gui.update(self.HISTORIC)

    def test_the_engine_knows_every_selectable_species(self):
        for species in SELECTABLE:
            #> Water's weight is the MW_H2O parameter rather than a literal,
            #> so it is absent from the parsed numbers by design and is
            #> checked by test_water_uses_the_shared_constant instead.
            if species != "h2o":
                self.assertIn(species, self.mw,
                              "%s can be selected in the interface but the "
                              "engine has no molecular weight for it, so it "
                              "would be given nitrous oxide's" % species)
            self.assertIn(species, self.dc,
                          "%s has no diffusivity in the engine" % species)

    def test_the_molecular_weights_match(self):
        for species in SELECTABLE:
            if species == "h2o":
                continue  #> engine uses the MW_H2O parameter, checked below
            gui_mw = self.gui[species][0] * 1e-3   #> g/mol -> kg/mol
            self.assertAlmostEqual(
                self.mw[species], gui_mw, places=9,
                msg="%s: engine %g kg/mol vs interface %g g/mol"
                    % (species, self.mw[species], self.gui[species][0]))

    def test_the_diffusivities_match(self):
        for species in SELECTABLE:
            gui_dc = self.gui[species][1] * 1e-4   #> cm2/s -> m2/s
            self.assertAlmostEqual(
                self.dc[species], gui_dc, places=12,
                msg="%s: engine %g m2/s vs interface %g cm2/s"
                    % (species, self.dc[species], self.gui[species][1]))

    def test_water_uses_the_shared_constant(self):
        """MW_H2O is a named parameter precisely so water's value cannot be
        typed twice; the table must reference it rather than repeat 18.02."""
        src = _read(ENGINE_SRC)
        body = src[src.index("function DefaultMolecularWeight(var)"):
                   src.index("end function DefaultMolecularWeight")]
        self.assertIn("MW_H2O", body)

    def test_an_unknown_species_is_reported(self):
        """Falling back must be loud. A wrong molecular weight produces a flux
        wrong by the same factor and nothing else looks unusual."""
        src = _read(ENGINE_SRC)
        self.assertIn("HasSpeciesDefaults", src)
        self.assertIn("ExceptionHandler(100)", src)


@unittest.skipUnless(ENGINE_SRC.exists(), "engine checkout not beside this one")
class TheInterfaceWritesWhatItShows(unittest.TestCase):
    """The values have to reach the record, or the engine never sees them.

    They used to go to setGeneralColGasMw / setGeneralColGasDiff, which set the
    project-wide gas_mw and gas_diff - keys writeMeasurementRecords deletes
    before saving. So every edit was discarded and the engine fell back to its
    own default, which is what made the table above matter so much.
    """

    def test_the_page_writes_the_record_not_the_retired_keys(self):
        #> Comment lines are dropped: the replacement documents what it
        #> replaced, so a naive search finds the retired name in prose.
        src = "\n".join(
            ln for ln in _read(GUI_ROOT / "src" / "basicsettingspage.cpp").splitlines()
            if not ln.lstrip().startswith("//")
        )
        self.assertNotIn("setGeneralColGasMw", src,
                         "the retired project-wide gas_mw key is deleted on "
                         "save; write GasRecord::mw instead")
        self.assertNotIn("setGeneralColGasDiff", src)
        for setter in ("setGasMolecularWeight", "setGasDiffusivity"):
            self.assertIn(setter, src)
        self.assertRegex(src, r"gases\[gasRecordIndex\]\.mw\s*=")
        self.assertRegex(src, r"gases\[gasRecordIndex\]\.diff\s*=")


if __name__ == "__main__":
    unittest.main()
