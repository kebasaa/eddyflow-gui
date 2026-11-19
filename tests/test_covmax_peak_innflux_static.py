"""The smoothed covariance peak search after innFLUX, end to end in the GUI.

innFLUX: Striednig et al. (2020), Atmos. Meas. Tech. 13, 1447-1465
(find_covariance_peak.m, v1.2.0); smoothing before the peak search after
Taipale et al. (2010). Two RawProcess_Settings keys, both off by default:

  covmax_smooth_len  Hamming window in rows, 0 = off
  covmax_peak_sign   0 largest |cov| (EddyFlow's search), 1 largest cov,
                     2 innFLUX's derivative rule

The engine side is pinned by eddyflow-engine's
static_checks/test_covmax_peak_innflux_static.py.
"""

from pathlib import Path
import re
import unittest

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE = GUI_ROOT.parent / "eddyflow-engine"


def read(*parts):
    return GUI_ROOT.joinpath(*parts).read_text(encoding="utf-8", errors="replace")


INIDEFS = read("src", "ecinidefs.h")
STATE = read("src", "ecprojectstate.h")
PROJECT = read("src", "ecproject.cpp")
PAGE = read("src", "advprocessingoptions.cpp")

KEYS = {"INI_SCREEN_SETTINGS_125": "covmax_smooth_len",
        "INI_SCREEN_SETTINGS_126": "covmax_peak_sign"}


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
            self.assertIn(f"value(EcIni::{const}", PROJECT)
            self.assertIn(f"screenSetting.{key} == previousProject", PROJECT)
            self.assertIn(f"screenSetting.{key} = defaultEcProjectState.screenSetting.{key}",
                          PROJECT)

    def test_sanitised_on_load(self):
        self.assertIn("covmax_smooth_len = (n < 2 || n > 999) ? 0 : n", PROJECT)
        self.assertIn("covmax_peak_sign = (n < 0 || n > 2) ? 0 : n", PROJECT)


class ThePage(unittest.TestCase):
    def test_controls_exist(self):
        self.assertIn("covmaxSmoothSpin = new QSpinBox", PAGE)
        self.assertIn("covmaxPeakSignCombo = new QComboBox", PAGE)

    def test_off_reads_off(self):
        self.assertIn('covmaxSmoothSpin->setSpecialValueText(tr("Off"))', PAGE)

    def test_written_on_the_users_edit_only(self):
        self.assertIn("QSignalBlocker b1(covmaxSmoothSpin)", PAGE)
        self.assertIn("QSignalBlocker b2(covmaxPeakSignCombo)", PAGE)
        self.assertIn("QOverload<int>::of(&QComboBox::activated)", PAGE)

    def test_greyed_with_the_baseline_box(self):
        m = re.search(r"void AdvProcessingOptions::updateCovmaxDebaselineAvailability\(\)"
                      r"\s*\{(.*?)\n\}", PAGE, re.S)
        self.assertIsNotNone(m)
        for w in ("covmaxSmoothSpin", "covmaxPeakSignCombo"):
            self.assertIn(f"{w}->setEnabled(meth == 2 || meth == 3)", m.group(1))

    def test_the_source_is_credited(self):
        """Every control tells the user where the method comes from."""
        m = re.search(r'const auto innfluxPeakCredit = tr\((.*?)"\);', PAGE, re.S)
        self.assertIsNotNone(m)
        credit = m.group(1)
        self.assertIn("Striednig et al. ", credit)
        self.assertIn("(2020)", credit)
        self.assertIn("Taipale et al. (2010)", credit)
        self.assertGreaterEqual(PAGE.count("+ innfluxPeakCredit"), 4)


@unittest.skipUnless((ENGINE / "src" / "src_rp" / "read_ini_rp.f90").is_file(),
                     "engine checkout not beside the GUI")
class TheEngineReadsTheSameKeys(unittest.TestCase):
    def test_same_labels(self):
        tags = (ENGINE / "src" / "src_rp" / "m_rp_global_var.f90").read_text(
            encoding="utf-8", errors="replace")
        for key in KEYS.values():
            self.assertRegex(tags, rf"SNTags\(\d+\)%Label\s*/\s*'{key}'")


if __name__ == "__main__":
    unittest.main()
