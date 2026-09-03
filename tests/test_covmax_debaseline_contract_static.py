"""Baseline-subtracted lag selection agrees across the interface and engine.

``covmax_debaseline`` is written into ``[RawProcess_Settings]``, which the
engine sweeps with the prefix ``RawProcess``. Spelled differently on the two
sides it is simply not found, the engine keeps its default of off, and the
box does nothing.

Two behaviours are pinned beyond the key itself.

**It is greyed unless the method maximises a covariance.** It modifies the
search, so it means nothing to ``Constant``, which does not search, nor to the
optimiser or the block-bootstrap, which choose their lags by other machinery.

**Its tooltip says what it does and does not change.** It selects a lag; the
covariance reported at that lag is computed exactly as before. A control in
the corrections half of a page reads like a correction unless it says
otherwise.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_TAGS = ENGINE_ROOT / "src" / "src_rp" / "m_rp_global_var.f90"
ENGINE_READER = ENGINE_ROOT / "src" / "src_rp" / "read_ini_rp.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


INIDEFS = read(GUI_ROOT / "src" / "ecinidefs.h")
STATE = read(GUI_ROOT / "src" / "ecprojectstate.h")
PROJECT = read(GUI_ROOT / "src" / "ecproject.cpp")
PAGE = read(GUI_ROOT / "src" / "advprocessingoptions.cpp")

KEY = "covmax_debaseline"
CONST = "INI_SCREEN_SETTINGS_107"
SLOT = 79

engine_available = ENGINE_TAGS.is_file() and ENGINE_READER.is_file()


class TheKeyReachesTheEngine(unittest.TestCase):

    def test_the_gui_declares_it(self):
        self.assertRegex(
            INIDEFS,
            r'const auto %s\s*=\s*QStringLiteral\("%s"\)' % (CONST, KEY))

    def test_the_gui_writes_and_reads_it(self):
        self.assertIn("setValue(EcIni::%s" % CONST, PROJECT)
        self.assertIn("value(EcIni::%s" % CONST, PROJECT)

    def test_it_defaults_to_off_in_the_interface(self):
        self.assertRegex(STATE, r"\bint %s = 0;" % KEY)

    def test_a_change_invalidates_a_computed_dataset(self):
        #> It moves every gas's time lag, so a run made before it was toggled
        #> is not the same run.
        self.assertIn(
            "ec_project_state_.screenSetting.%s == "
            "previousProject.ec_project_state_.screenSetting.%s" % (KEY, KEY),
            PROJECT)

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_declares_it_at_the_expected_slot(self):
        self.assertRegex(
            read(ENGINE_TAGS),
            r"SCTags\(%d\)%%Label\s*/\s*'%s'\s*/" % (SLOT, KEY))

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_reads_it_under_a_found_guard(self):
        #> A new character tag read blind would carry whatever the shared
        #> array held from the previous parse.
        self.assertIn("SCTagFound(%d)" % SLOT, read(ENGINE_READER))


class TheBoxIsOfferedOnlyWhereItApplies(unittest.TestCase):

    def test_it_is_greyed_by_the_method(self):
        fn = re.search(
            r"void AdvProcessingOptions::updateCovmaxDebaselineAvailability\(\)"
            r"\s*\{(.*?)\n\}", PAGE, re.S)
        self.assertIsNotNone(fn, "no availability helper")
        body = fn.group(1)
        #> tlag_meth 2 is maxcov&default and 3 is maxcov. Constant (1), the
        #> optimiser (4) and the block-bootstrap (5) do not search this way.
        self.assertIn("meth == 2 || meth == 3", body)

    def test_the_helper_is_called_from_both_handlers_and_refresh(self):
        self.assertGreaterEqual(
            PAGE.count("updateCovmaxDebaselineAvailability()"), 4,
            "the availability rule is not applied from every path that can "
            "change the method (declaration, two handlers, refresh)")

    def test_it_writes_on_click_not_on_state_change(self):
        #> refresh() blocks the project's signals, not the widgets'.
        m = re.search(
            r"connect\(covmaxDebaselineCheckBox, &RichTextCheckBox::(\w+)",
            PAGE)
        self.assertIsNotNone(m)
        self.assertEqual(m.group(1), "clicked")


class TheTooltipSaysWhatItDoesNotChange(unittest.TestCase):

    def setUp(self):
        m = re.search(r"covmaxDebaselineCheckBox->setToolTip\(tr\((.*?)\)\);",
                      PAGE, re.S)
        self.assertIsNotNone(m, "the checkbox has no tooltip")
        #> Adjacent C++ string literals, joined: a sentence in the tooltip
        #> spans several of them and would otherwise be unsearchable.
        self.tip = "".join(re.findall(r'"([^"]*)"', m.group(1)))

    def test_it_says_it_selects_a_lag_not_a_correction(self):
        self.assertIn("which lag is selected", self.tip)
        self.assertIn("nothing about the covariance", self.tip)

    def test_it_warns_that_the_default_fallback_stops_firing(self):
        #> With the baseline removed the window ends score zero by
        #> construction, so the maximum can never land on one - and
        #> maxcov&default falls back precisely when it does. Anyone relying
        #> on that safety net needs to be told it has gone.
        self.assertIn("never land on an end", self.tip)
        self.assertIn("nominal", self.tip)


if __name__ == "__main__":
    unittest.main()
