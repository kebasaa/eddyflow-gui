"""Which humidity corrects a gas is the user's choice, per gas.

A site can have two hygrometers and a biomet RH sensor, and until now the
engine decided between them on its own: biomet overwrote the *primary*
hygrometer's reported humidity and nothing else, so which instrument's numbers
were real followed the primary designation. Widening that to every hygrometer
only replaced one blanket rule with another.

So the Moisture column offers the biomet alongside the analyser channels, and a
tickbox sets every gas to it at once for the common case. Three things this
file exists to hold in place:

  - **The sentinel is one number written down twice.** `moistureRef = -1` means
    the biomet here and in the engine's `biometMoistRef`, and the project file
    carries it between them. Anything that treats a negative reference as
    corruption - `validateReferences` did - turns "use the biomet" into "work
    it out yourself" on the next load, and the dropdown appears to forget.

  - **`resolveMoistureRef` mirrors `ResolveGasRef`.** Explicit choice, then the
    gas's own analyser, then the biomet. The interface shows what the engine
    will use, or it is lying about the fluxes.

  - **The tickbox is the only action with a consequence.** Selecting a biomet
    RH column makes the biomet *available*; it overrides nothing by itself, so
    it raises nothing. Ticking the box changes every gas, and says so.

The engine never reads `biom_rh_override`. It acts on the per-gas references
the box sets, and the key exists only so the box comes back ticked.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]

PAGE = "src/basicsettingspage.cpp"
PAGE_H = "src/basicsettingspage.h"
PROJECT = "src/ecproject.cpp"
RECORD_H = "src/measurement_record.h"
RECORD = "src/measurement_record.cpp"


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def code(path):
    out = []
    for ln in read(path).splitlines():
        s = ln.lstrip()
        if s.startswith("//") or s.startswith("/*") or s.startswith("*"):
            continue
        out.append(ln)
    return "\n".join(out)


def body(path, signature):
    src = code(path)
    start = src.index(signature)
    depth = 0
    seen = False
    for i in range(start, len(src)):
        if src[i] == "{":
            depth += 1
            seen = True
        elif src[i] == "}":
            depth -= 1
            if seen and depth == 0:
                return src[start:i + 1]
    raise AssertionError("unterminated body for %s" % signature)


class TheSentinelSurvivesTheRoundTrip(unittest.TestCase):
    def test_it_is_a_named_constant(self):
        self.assertIn("inline constexpr int biometMoistureRef = -1;",
                      code(RECORD_H))

    def test_nothing_uses_the_bare_literal(self):
        """A -1 spelled out is a -1 nobody can grep for when the engine's
        matching constant changes."""
        for path in (PAGE, PROJECT, RECORD):
            src = code(path)
            self.assertNotIn("moistureRef = -1", src,
                             "%s must go through the named constant" % path)

    def test_validation_does_not_reset_it(self):
        """It used to zero every negative reference, which would silently undo
        the user's choice on each load."""
        block = body(RECORD, "void validateReferences(")
        self.assertIn("gas.moistureRef != biometMoistureRef", block,
                      "the biomet sentinel is a valid reference, not a "
                      "corrupt index, and must survive validation")

    def test_the_engine_constant_is_named_in_the_comment(self):
        """One value, two files, joined only by the project file."""
        self.assertIn("biometMoistRef", read(RECORD_H))


class ResolutionMirrorsTheEngine(unittest.TestCase):
    def setUp(self):
        self.block = body(RECORD, "int resolveMoistureRef(")

    def test_an_explicit_biomet_choice_wins(self):
        self.assertIn("gas.moistureRef == biometMoistureRef", self.block)

    def test_the_own_analyser_hygrometer_comes_next(self):
        self.assertIn("gases.at(i).instrumentId == gas.instrumentId",
                      self.block)

    def test_the_last_rule_is_the_biomet_not_a_borrowed_hygrometer(self):
        """The old fallback took the first H2O of any analyser, through a
        different cell at a different time lag."""
        self.assertIn("if (biometRhAvailable) { return biometMoistureRef; }",
                      self.block)
        self.assertNotIn("if (gases.at(i).slug == kH2o) { return i + 1; }",
                         self.block,
                         "borrowing another analyser's water is no longer an "
                         "automatic outcome")

    def test_the_order_is_own_analyser_before_biomet(self):
        self.assertLess(
            self.block.index("gases.at(i).instrumentId == gas.instrumentId"),
            self.block.index("if (biometRhAvailable) { return biometMoistureRef; }"))

    def test_callers_say_whether_a_biomet_exists(self):
        """Without the flag the last two rules cannot be answered, and the
        default of false would quietly report no moisture source at all."""
        src = code(PAGE)
        for m in re.finditer(r"resolveMoistureRef\(([^;]*?)\);", src, re.S):
            self.assertIn("biometRhAvailable()", m.group(1),
                          "every caller must pass the availability flag")


class TheDropdownOffersIt(unittest.TestCase):
    def setUp(self):
        self.block = body(PAGE, "QVector<QPair<int, QString>> BasicSettingsPage::moistureChoices() const")

    def test_only_when_the_project_has_one(self):
        self.assertIn("if (biometRhAvailable())", self.block)

    def test_it_is_keyed_on_the_biomet_column_numbering(self):
        """Biomet columns carry col + 1000 through this interface, so the entry
        cannot collide with a raw column and setMoistureColumnForGas can tell
        the two apart without a second parameter."""
        self.assertIn("ecProject_->biomParamColRh()", self.block)

    def test_one_spelling_shared_with_the_cell(self):
        """moistureLabelForGas finds the current entry by matching this text;
        two spellings leave the cell blank and the delegate reads that as
        nothing selected."""
        self.assertIn("biometMoistureLabel()", self.block)
        self.assertIn("biometMoistureLabel()",
                      body(PAGE, "QString BasicSettingsPage::moistureLabelForGas(int gasRecordIndex) const"))

    def test_selecting_it_stores_the_sentinel(self):
        block = body(PAGE, "bool BasicSettingsPage::setMoistureColumnForGas(int gasRecordIndex, int rawColumn)")
        self.assertIn("if (rawColumn > 1000)", block)
        self.assertIn("MeasurementRecords::biometMoistureRef", block)


class TheTickboxIsTheOnlyActionThatChangesAnything(unittest.TestCase):
    def test_selecting_an_rh_column_raises_nothing(self):
        block = body(PAGE, "void BasicSettingsPage::updateRhCombo(int i)")
        self.assertNotIn("warnOnBiometRhOverride", block,
                         "picking a biomet RH column overrides nothing on its "
                         "own; it only makes the biomet available")

    def test_the_box_raises_it_on_the_way_in_only(self):
        block = body(PAGE, "void BasicSettingsPage::onBiometRhOverrideToggled(bool on)")
        self.assertIn("if (on) { warnOnBiometRhOverride(); }", block)

    def test_ticking_sets_every_gas(self):
        block = body(PROJECT, "bool EcProject::setBiometRhOverride(bool on)")
        self.assertIn("for (auto& gas : gases)", block)
        self.assertIn("gas.moistureRef = want", block)

    def test_unticking_returns_them_to_automatic(self):
        block = body(PROJECT, "bool EcProject::setBiometRhOverride(bool on)")
        self.assertIn("on ? MeasurementRecords::biometMoistureRef : 0", block)

    def test_the_box_is_disabled_without_a_biomet_column(self):
        block = body(PAGE, "void BasicSettingsPage::refreshBiometRhOverrideBox()")
        self.assertIn("setEnabled(biometRhAvailable())", block)

    def test_rebuilding_the_box_is_not_mistaken_for_a_click(self):
        block = body(PAGE, "void BasicSettingsPage::refreshBiometRhOverrideBox()")
        self.assertIn("QSignalBlocker", block)

    def test_the_tables_are_rebuilt_after_a_toggle(self):
        """Every Moisture cell just changed."""
        block = body(PAGE, "void BasicSettingsPage::onBiometRhOverrideToggled(bool on)")
        self.assertIn("refreshVariableTables()", block)


class TheKeyIsForTheInterfaceAlone(unittest.TestCase):
    def test_it_is_written_and_read_back(self):
        src = code(PROJECT)
        self.assertIn("EcIni::INI_BIOMET_RH_OVERRIDE", src)
        self.assertEqual(2, src.count("INI_BIOMET_RH_OVERRIDE"),
                         "written once, read once")

    def test_the_write_site_says_the_engine_ignores_it(self):
        """Every other key in that group is read by the engine, and a reader
        will assume this one is."""
        text = read(PROJECT)
        at = text.index("INI_BIOMET_RH_OVERRIDE")
        preamble = text[max(0, at - 600): at]
        self.assertIn("engine", preamble)

    def test_the_engine_does_not_read_it(self):
        engine = ROOT.parent / "eddyflow-engine" / "src"
        if not engine.is_dir():
            self.skipTest("engine tree not beside this one")
        hits = [p for p in engine.rglob("*.f90")
                if "biom_rh_override" in p.read_text(encoding="utf-8",
                                                     errors="replace")]
        self.assertEqual([], hits,
                         "the engine acts on the per-gas references, not on "
                         "this key")


class TheTextSaysWhatMovesAndWhatDoesNot(unittest.TestCase):
    def setUp(self):
        text = read(PAGE)
        start = text.index("void BasicSettingsPage::warnOnBiometRhOverride()")
        plain = text[start: start + 1800]
        #> Join the adjacent string literals before matching. A sentence split
        #> across a line break is still that sentence, and an assertion that
        #> fails on where the wrap landed tests the formatting rather than the
        #> message.
        plain = re.sub(r'"\s*"', "", plain)
        plain = re.sub(r"<[^>]+>", "", plain)
        self.plain = re.sub(r"\s+", " ", plain)

    def test_it_says_every_gas(self):
        self.assertIn("Every gas", self.plain)

    def test_it_names_the_corrections_that_change(self):
        for term in ("WPL", "drift", "LI-7700"):
            self.assertIn(term, self.plain)

    def test_it_says_the_hygrometers_still_report_themselves(self):
        self.assertIn("still report what they measured", self.plain)
        self.assertIn("h2o_biomet", self.plain)

    def test_it_says_unticking_does_not_undo(self):
        self.assertIn("cannot restore", self.plain)


if __name__ == "__main__":
    unittest.main()
