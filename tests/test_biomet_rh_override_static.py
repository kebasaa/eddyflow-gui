"""Selecting a biomet RH column replaces what the hygrometers measured.

The engine treats a biomet relative humidity sensor as the site's humidity: it
reports every hygrometer's mole fraction, mixing ratio and molar density from
that value rather than from the instrument's own channel, and it WPL-corrects
every gas with it. That is usually the right call - a calibrated RH sensor is
steadier than an analyser's water channel over a long deployment - but nothing
told the user it was happening, and a column named `h2o_1_mixing_ratio` reading
biomet rather than the LI-7200 is not something anyone would guess.

So: a dialog when the user turns it on, a triangle on the RH row when a project
arrives with it already on. That split is the rule established for the
cross-analyser water warning, and for the same reason - a dialog about a
decision nobody just took is noise, and the second time it appears the user has
learned to dismiss it without reading.

The condition has two halves and both matter. A biomet RH column with no
hygrometer is not this case at all: there is nothing to override, and
`showNoHumidityWarning` tells the user that a biomet RH is exactly what their
project needs. The two warnings are opposites. They test the same two things
and must never both appear, which is only reliable while they share the test.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]

PAGE = "src/basicsettingspage.cpp"
PAGE_H = "src/basicsettingspage.h"


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


class OnePredicateForAllThree(unittest.TestCase):
    """Dialog, triangle and tooltip. Three copies of a condition is how the
    mark comes to appear where the message does not, or the reverse."""

    def test_the_predicate_exists(self):
        self.assertIn("bool BasicSettingsPage::biometRhOverridesHygrometers() const",
                      code(PAGE))

    def test_it_tests_both_halves(self):
        block = body(PAGE, "bool BasicSettingsPage::biometRhOverridesHygrometers() const")
        self.assertIn("biomParamColRh()", block)
        self.assertIn('QLatin1String("h2o")', block,
                      "a biomet RH with no hygrometer is not this case - it is "
                      "the case showNoHumidityWarning is about")

    def test_every_consumer_asks_it(self):
        src = code(PAGE)
        #> The predicate's own definition, the dialog, the model helper, and
        #> the transition test in updateRhCombo.
        self.assertGreaterEqual(src.count("biometRhOverridesHygrometers()"), 4)
        self.assertIn("page_->biometRhOverridesHygrometers()", src,
                      "the table must ask the page rather than re-deriving it")

    def test_the_two_warnings_cannot_both_fire(self):
        """showNoHumidityWarning returns early when a biomet RH column is
        selected; this one returns early when no gas record is water. They
        partition the space."""
        other = body(PAGE, "void BasicSettingsPage::showNoHumidityWarning()")
        self.assertIn("biomParamColRh() > 0) { return; }", other)


class TheDialogAnswersAnAction(unittest.TestCase):
    def test_it_is_raised_from_the_combo_handler(self):
        block = body(PAGE, "void BasicSettingsPage::updateRhCombo(int i)")
        self.assertIn("warnOnBiometRhOverride()", block)

    def test_only_on_the_transition_into_the_override(self):
        """Re-picking a different biomet RH column while already overriding
        changes which sensor, not whether the hygrometers are replaced."""
        block = body(PAGE, "void BasicSettingsPage::updateRhCombo(int i)")
        self.assertIn("const bool wasOverriding = biometRhOverridesHygrometers();",
                      block)
        self.assertIn("if (!wasOverriding && biometRhOverridesHygrometers())",
                      block)
        self.assertLess(block.index("wasOverriding = "),
                        block.index("setBiomParamColRh"),
                        "the before-state must be sampled before the project "
                        "is changed, or it reads the after-state")

    def test_no_load_path_raises_it(self):
        """Opening a project must not raise it - the triangle covers that."""
        src = code(PAGE)
        for caller in ("void BasicSettingsPage::reloadSelectedItems_1()",
                       "void BasicSettingsPage::updateMetadataRead("):
            if caller not in src:
                continue
            self.assertNotIn("warnOnBiometRhOverride", body(PAGE, caller),
                             "%s must not raise a dialog about a state the "
                             "user did not just create" % caller)

    def test_it_has_no_once_per_session_flag(self):
        """It fires on an action, and an action repeated deserves the same
        answer. The flag belongs to the load-time warnings."""
        block = body(PAGE, "void BasicSettingsPage::warnOnBiometRhOverride()")
        self.assertNotIn("Warned_", block)

    def test_the_table_is_refreshed_so_the_triangle_follows(self):
        block = body(PAGE, "void BasicSettingsPage::updateRhCombo(int i)")
        self.assertIn("refreshVariableTables()", block)


class TheTextSaysWhatMovesAndWhatDoesNot(unittest.TestCase):
    """Getting either half wrong sends the user looking in the wrong place."""

    def setUp(self):
        self.text = read(PAGE)
        start = self.text.index("void BasicSettingsPage::warnOnBiometRhOverride()")
        self.block = self.text[start: start + 2200]

    def test_it_names_the_columns_that_change(self):
        for term in ("mole fraction", "mixing ratio", "molar density", "WPL"):
            self.assertIn(term, self.block)

    def test_it_says_every_hygrometer_not_just_one(self):
        """The whole point of the engine change. Bare "every" is not enough to
        assert on - the text says "every gas" too, so dropping the quantifier
        from the hygrometer clause left this passing."""
        plain = re.sub(r"<[^>]+>", "", self.block)
        plain = re.sub(r"\s+", " ", plain)
        self.assertIn("every hygrometer", plain)

    def test_it_says_the_fluxes_are_untouched(self):
        self.assertIn("not affected", self.block)
        for term in ("LE", "ET", "covariance"):
            self.assertIn(term, self.block)

    def test_it_says_how_to_turn_it_off(self):
        self.assertIn("Deselect", self.block)


class TheTriangleMarksTheAlreadySetCase(unittest.TestCase):
    def test_the_decoration_is_on_the_rh_row(self):
        src = code(PAGE)
        self.assertIn("bool biometRhOverride(const VariableTableCandidate& row) const",
                      src)
        block = body(PAGE, "bool biometRhOverride(const VariableTableCandidate& row) const")
        self.assertIn("row.row.role != VariableTableRole::Rh", block)

    def test_it_is_on_the_variable_column(self):
        """paintComboCell fills the combo-painted columns from DisplayRole
        alone, so a decoration there is discarded and reads as working."""
        src = code(PAGE)
        m = re.search(r"if \(index\.column\(\) == Variable && biometRhOverride\(row\)\)", src)
        self.assertTrue(m, "the RH decoration must be on the Variable column")

    def test_it_reuses_the_existing_icon(self):
        src = code(PAGE)
        start = src.index("biometRhOverride(row))")
        self.assertIn("crossAnalyserIcon()", src[start: start + 200])

    def test_the_tooltip_explains_the_mark(self):
        src = code(PAGE)
        self.assertIn("if (biometRhOverride(row))", src)
        text = read(PAGE)
        at = text.index("This biomet humidity replaces what every")
        tip = text[at: at + 500]
        self.assertIn("hygrometer", tip)
        self.assertIn("unaffected", tip)


class ItIsDeclared(unittest.TestCase):
    def test_the_predicate_is_reachable_from_the_table(self):
        """The model holds a BasicSettingsPage* and calls through it."""
        h = read(PAGE_H)
        at = h.index("bool biometRhOverridesHygrometers() const;")
        before = h[:at]
        self.assertGreater(before.rindex("public:"),
                           before.rindex("private:") if "private:" in before else -1,
                           "the predicate must be public for the table model "
                           "to call it")


if __name__ == "__main__":
    unittest.main()
