"""Planar-fit extras after innFLUX in the GUI: moving windows and the bootstrap.

Striednig et al. (2020), Sect. 2.3.1 and Fig. 1, after Wilczak et al. (2001).
Both off by default. The engine side is pinned by eddyflow-engine's
static_checks/test_pf_extras_static.py.
"""

from pathlib import Path
import unittest

GUI_ROOT = Path(__file__).resolve().parents[1]


def read(*parts):
    return GUI_ROOT.joinpath(*parts).read_text(encoding="utf-8", errors="replace")


INIDEFS = read("src", "ecinidefs.h")
STATE = read("src", "ecprojectstate.h")
PROJECT = read("src", "ecproject.cpp")
DIALOG = read("src", "planarfitsettingsdialog.cpp")

KEYS = {"INI_SCREEN_TILT_15": "pf_mw_num", "INI_SCREEN_TILT_16": "pf_mw_width",
        "INI_SCREEN_TILT_17": "pf_boot_n", "INI_SCREEN_TILT_18": "pf_boot_seed"}


class TheProject(unittest.TestCase):
    def test_keys(self):
        for const, key in KEYS.items():
            self.assertRegex(INIDEFS, rf'{const}\s*=\s*QStringLiteral\("{key}"\)')
            self.assertIn(f"setValue(EcIni::{const}", PROJECT)
            self.assertIn(f"value(EcIni::{const}", PROJECT)

    def test_off_by_default(self):
        self.assertIn("int mw_num = 0;", STATE)
        self.assertIn("int boot_n = 0;", STATE)

    def test_compared_and_reset(self):
        self.assertIn("screenTilt.mw_num == previousProject", PROJECT)
        self.assertIn("screenTilt.boot_n = defaultEcProjectState.screenTilt.boot_n", PROJECT)


class TheDialog(unittest.TestCase):
    def test_controls_off_by_default(self):
        self.assertIn('mwNumSpin->setSpecialValueText(tr("Off (use the sectors)"))', DIALOG)
        self.assertIn('bootSpin->setSpecialValueText(tr("Off"))', DIALOG)

    def test_loading_writes_nothing(self):
        self.assertIn("if (extrasRefreshing_) { return; }", DIALOG)

    def test_credited(self):
        self.assertIn("Striednig et al. \"\n            \"(2020)", DIALOG)
        self.assertIn("Wilczak et al. (2001)", DIALOG)
        self.assertGreaterEqual(DIALOG.count("+ credit);"), 4)


if __name__ == "__main__":
    unittest.main()
