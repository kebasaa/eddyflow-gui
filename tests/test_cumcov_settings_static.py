"""The cumulated-covariance time lag (tlag_meth 6) in the GUI.

After innFLUX (Striednig et al. 2020, after Park et al. 2013); clock drift
after innFLUX v1.2.0's driftfix (unpublished). The engine side is pinned by
eddyflow-engine's static_checks/test_cumcov_static.py.
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
DIALOG = read("src", "cumcovsettingsdialog.cpp")
RECORD = read("src", "measurement_record.h")
CMAKE = read("CMakeLists.txt")

KEYS = ["tlag_cum_filter_len", "tlag_cum_min_ustar", "tlag_cum_min_compl_sonic",
        "tlag_cum_min_compl_gas", "tlag_cum_max_sst", "tlag_cum_segments",
        "tlag_drift_meth", "tlag_drift_span_d", "tlag_drift_jump_thr",
        "tlag_drift_max_s", "out_ccf_max_lag_s", "out_ccf_decim"]


class TheProject(unittest.TestCase):
    def test_keys_declared_saved_and_loaded(self):
        for n, key in enumerate(KEYS, start=129):
            const = f"INI_SCREEN_SETTINGS_{n}"
            self.assertRegex(INIDEFS, rf'{const}\s*=\s*QStringLiteral\("{key}"\)')
            self.assertIn(f"setValue(EcIni::{const}", PROJECT)
            self.assertIn(f"value(EcIni::{const}", PROJECT)

    def test_innflux_defaults(self):
        for literal in ("int filter_len = 15;", "qreal min_ustar = 0.3;",
                        "qreal min_compl_sonic = 0.95;", "qreal min_compl_gas = 0.5;",
                        "qreal max_sst = 0.3;", "int drift_meth = 0;",
                        "qreal drift_span_d = 5.0;", "qreal drift_jump_thr = -0.8;"):
            self.assertIn(literal, STATE)

    def test_per_gas_flags_unstated_by_default(self):
        self.assertIn("int tlagDriftRef = -1;", RECORD)
        self.assertIn("int outCcf = -1;", RECORD)
        self.assertIn('QStringLiteral("tlag_drift_ref")', PROJECT)
        self.assertIn("if (proc.tlagDriftRef >= 0)", PROJECT)

    def test_compared_and_reset(self):
        self.assertIn("screenSetting.cumcov == previousProject", PROJECT)
        self.assertIn("screenSetting.cumcov = defaultEcProjectState.screenSetting.cumcov",
                      PROJECT)


class ThePage(unittest.TestCase):
    def test_method_is_the_sixth(self):
        items = re.findall(r'timeLagMethodCombo->addItem\(tr\("([^"]+)"\)\)', PAGE)
        self.assertEqual(len(items), 6)
        self.assertIn("Cumulated covariance", items[5])
        self.assertIn("Striednig et al. 2020", items[5])

    def test_settings_button_and_dialog(self):
        self.assertIn("tlSettingsButton->setEnabled(n == 3 || n == 4 || n == 5)", PAGE)
        self.assertIn("if (ecProject_->screenTlagMeth() == 6)", PAGE)
        self.assertIn("src/cumcovsettingsdialog.cpp", CMAKE)
        self.assertIn("src/cumcovsettingsdialog.h", CMAKE)

    def test_the_dialog_credits_its_sources(self):
        self.assertIn("Striednig et al. (2020)", DIALOG)
        self.assertIn("Park et al. ", DIALOG)
        self.assertIn("innFLUX v1.2.0", DIALOG)
        self.assertIn("unpublished", DIALOG)
        self.assertGreaterEqual(DIALOG.count("+ credit()"), 6)
        self.assertGreaterEqual(DIALOG.count("+ driftCredit()"), 5)

    def test_loading_writes_nothing(self):
        self.assertIn("if (refreshing_) { return; }", DIALOG)
        self.assertIn("const QSignalBlocker blocker(refList);", DIALOG)


class TheCcfOutput(unittest.TestCase):
    """Per-period cross-covariance functions (innFLUX step 1 files), by gas."""

    def setUp(self):
        self.page = read("src", "advoutputoptions.cpp")

    def test_list_half_width_and_decimation(self):
        self.assertIn("ccfGasList = new QListWidget", self.page)
        self.assertIn("c.ccf_max_lag_s = v;", self.page)
        self.assertIn("c.ccf_decim = v;", self.page)

    def test_unticked_is_not_stated(self):
        self.assertIn("== Qt::Checked ? 1 : -1;", self.page)
        self.assertIn('QStringLiteral("out_ccf")', PROJECT)

    def test_loading_writes_nothing(self):
        self.assertGreaterEqual(self.page.count("if (ccfRefreshing_) { return; }"), 3)

    def test_credited(self):
        self.assertIn("Striednig et al. (2020)", self.page[self.page.index("ccfCredit"):][:400])


@unittest.skipUnless((ENGINE / "src" / "src_rp" / "m_rp_global_var.f90").is_file(),
                     "engine checkout not beside the GUI")
class TheEngineReadsTheSameKeys(unittest.TestCase):
    def test_same_labels(self):
        tags = (ENGINE / "src" / "src_rp" / "m_rp_global_var.f90").read_text(
            encoding="utf-8", errors="replace")
        for key in KEYS:
            self.assertRegex(tags, rf"(SNTags|SCTags)\(\d+\)%Label\s*/\s*'{key}'")


if __name__ == "__main__":
    unittest.main()
