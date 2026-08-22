"""Conditional lag borrowing agrees across the interface and the engine.

Nemitz et al. (2018): a gas whose covariance cannot be told from noise takes
a tube-mate's time lag. Two settings, in ``[RawProcess_Settings]``, which the
engine sweeps with the prefix ``RawProcess``.

The dependency is the thing worth pinning. Borrowing divides a covariance by a
detection limit, and ``detlim_meth`` lives on a *different page* - Statistical
Analysis. So the control here can be greyed by something the user changed on
another tab, and the engine refuses the same combination independently. Both
halves have to stay in step, and the tooltip has to say where to go.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_TAGS = ENGINE_ROOT / "src" / "src_rp" / "m_rp_global_var.f90"
ENGINE_READER = ENGINE_ROOT / "src" / "src_rp" / "read_ini_rp.f90"
ENGINE_BORROW = ENGINE_ROOT / "src" / "src_rp" / "borrow_timelag.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


INIDEFS = read(GUI_ROOT / "src" / "ecinidefs.h")
STATE = read(GUI_ROOT / "src" / "ecprojectstate.h")
PROJECT = read(GUI_ROOT / "src" / "ecproject.cpp")
PAGE = read(GUI_ROOT / "src" / "advprocessingoptions.cpp")

KEYS = {
    "tlag_borrow_meth": ("INI_SCREEN_SETTINGS_108", 80, "SCTags"),
    "tlag_borrow_snr": ("INI_SCREEN_SETTINGS_109", 60, "SNTags"),
}

engine_available = ENGINE_TAGS.is_file() and ENGINE_READER.is_file()


class TheKeysReachTheEngine(unittest.TestCase):

    def test_the_gui_declares_writes_and_reads_each(self):
        for key, (const, _, _) in KEYS.items():
            self.assertRegex(
                INIDEFS,
                r'const auto %s\s*=\s*QStringLiteral\("%s"\)' % (const, key))
            self.assertIn("setValue(EcIni::%s" % const, PROJECT)
            self.assertIn("value(EcIni::%s" % const, PROJECT)

    def test_the_defaults_are_off_and_three(self):
        self.assertRegex(STATE, r"\bint tlag_borrow_meth = 0;")
        self.assertRegex(STATE, r"\bqreal tlag_borrow_snr = 3\.0;")

    def test_the_names_do_not_nest(self):
        #> The engine's tag table forbids one label being a substring of
        #> another in the same scope. `tlag_borrow` was renamed to
        #> `tlag_borrow_meth` for exactly this reason.
        a, b = "tlag_borrow_meth", "tlag_borrow_snr"
        self.assertNotIn(a, b)
        self.assertNotIn(b, a)

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_declares_each_at_the_expected_slot(self):
        tags = read(ENGINE_TAGS)
        for key, (_, slot, table) in KEYS.items():
            self.assertRegex(
                tags, r"%s\(%d\)%%Label\s*/\s*'%s'\s*/" % (table, slot, key),
                "the engine does not hold %r at %s(%d)" % (key, table, slot))

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_threshold_default_matches_the_engines(self):
        self.assertRegex(read(ENGINE_READER), r"RPSetup%tlag_borrow_snr = 3d0")


class TheDependencyOnTheDetectionLimitHolds(unittest.TestCase):

    def test_the_interface_greys_the_controls_without_it(self):
        fn = re.search(
            r"void AdvProcessingOptions::updateTlagBorrowAvailability\(\)"
            r"\s*\{(.*?)\n\}", PAGE, re.S)
        self.assertIsNotNone(fn, "no availability helper")
        body = fn.group(1)
        self.assertIn("screenDetlimMethod() > 0", body)
        self.assertIn("tlagBorrowCheckBox->setEnabled(haveLimit)", body)

    def test_the_helper_runs_from_every_path_that_can_change_it(self):
        #> Declaration, the checkbox handler, refresh and reset.
        self.assertGreaterEqual(
            PAGE.count("updateTlagBorrowAvailability()"), 4)

    @unittest.skipUnless(ENGINE_BORROW.is_file(), "engine repo not beside the GUI")
    def test_the_engine_refuses_the_same_combination(self):
        #> Independently of the interface: a hand-written project can state
        #> borrowing without a detection limit, and must not get it.
        self.assertIn(
            "if (RPSetup%detlim_meth /= 'wienhold_94') return",
            read(ENGINE_BORROW))

    def test_the_tooltip_says_where_to_switch_the_limit_on(self):
        #> It is on another page, so "requires the detection limit" alone
        #> would leave the user hunting.
        m = re.search(r"tlagBorrowCheckBox->setToolTip\(tr\((.*?)\)\);",
                      PAGE, re.S)
        self.assertIsNotNone(m, "the checkbox has no tooltip")
        tip = "".join(re.findall(r'"([^"]*)"', m.group(1)))
        self.assertIn("Requires the flux detection limit", tip)
        self.assertIn("Statistical Analysis", tip)

    def test_the_tooltip_states_what_is_never_borrowed(self):
        m = re.search(r"tlagBorrowCheckBox->setToolTip\(tr\((.*?)\)\);",
                      PAGE, re.S)
        tip = "".join(re.findall(r'"([^"]*)"', m.group(1)))
        self.assertIn("Water is never borrowed", tip)
        self.assertIn("best-resolved", tip)


class TheChangeInvalidatesADataset(unittest.TestCase):

    def test_both_settings_are_in_the_comparison(self):
        for key in KEYS:
            self.assertIn(
                "screenSetting.%s" % key, PROJECT,
                "%s is not compared, so toggling it would not mark the "
                "computed dataset stale" % key)


if __name__ == "__main__":
    unittest.main()
