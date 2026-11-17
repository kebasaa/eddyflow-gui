"""Optional FLUXNET columns after innFLUX, end to end in the GUI.

innFLUX: Striednig et al. (2020), Atmos. Meas. Tech. 13, 1447-1465. Two
RawProcess_Settings switches, both off by default:

  out_theta_flux       W_THETA and W_THETA_V
  out_lod_noise_stats  far-lag noise mean/SD of the Langford limit of
                       detection and the flux signal-to-noise ratio

The engine side is pinned by eddyflow-engine's
static_checks/test_innflux_outputs_static.py.
"""

from pathlib import Path
import re
import unittest

GUI_ROOT = Path(__file__).resolve().parents[1]


def read(*parts):
    return GUI_ROOT.joinpath(*parts).read_text(encoding="utf-8", errors="replace")


INIDEFS = read("src", "ecinidefs.h")
STATE = read("src", "ecprojectstate.h")
PROJECT = read("src", "ecproject.cpp")
PAGE = read("src", "advoutputoptions.cpp")

KEYS = {"INI_SCREEN_SETTINGS_127": "out_theta_flux",
        "INI_SCREEN_SETTINGS_128": "out_lod_noise_stats"}


class TheProjectCarriesThem(unittest.TestCase):
    def test_declared(self):
        for const, key in KEYS.items():
            self.assertRegex(INIDEFS, rf'{const}\s*=\s*QStringLiteral\("{key}"\)')

    def test_off_by_default(self):
        for key in KEYS.values():
            self.assertRegex(STATE, rf"\bint {key} = 0;")

    def test_saved_loaded_compared_and_reset(self):
        for const, key in KEYS.items():
            self.assertIn(f"setValue(EcIni::{const}", PROJECT)
            self.assertIn(f"value(EcIni::{const}, 0).toInt() == 1 ? 1 : 0", PROJECT)
            self.assertIn(f"screenSetting.{key} == previousProject", PROJECT)
            self.assertIn(f"screenSetting.{key} = defaultEcProjectState.screenSetting.{key}",
                          PROJECT)


class ThePage(unittest.TestCase):
    def test_written_on_click(self):
        for box in ("outThetaFluxCheckBox", "outLodNoiseCheckBox"):
            self.assertRegex(PAGE, rf"connect\({box}, &RichTextCheckBox::clicked")

    def test_not_in_any_preset(self):
        """Off by default means a preset does not switch them on either."""
        for fn in ("selectMin", "selectMax", "selectMed"):
            m = re.search(rf"void AdvOutputOptions::{fn}\(\)\s*\{{(.*?)\n\}}", PAGE, re.S)
            if m:
                self.assertNotIn("outThetaFluxCheckBox", m.group(1))
                self.assertNotIn("outLodNoiseCheckBox", m.group(1))

    def test_the_source_is_credited(self):
        for box in ("outThetaFluxCheckBox", "outLodNoiseCheckBox"):
            m = re.search(rf"{box}->setToolTip\(tr\((.*?)\)\);", PAGE, re.S)
            self.assertIsNotNone(m, box)
            self.assertIn("Striednig et al. (2020)", m.group(1))
            self.assertIn("Off by", m.group(1))


if __name__ == "__main__":
    unittest.main()
