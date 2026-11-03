from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


class PwbGuiStaticIntegrationTests(unittest.TestCase):
    def test_dropdown_order_keeps_existing_methods_and_adds_pwb_fifth(self):
        source = read("src/advprocessingoptions.cpp")
        labels = [
            'timeLagMethodCombo->addItem(tr("Constant"))',
            'timeLagMethodCombo->addItem(tr("Covariance maximization with default"))',
            'timeLagMethodCombo->addItem(tr("Covariance maximization"))',
            'timeLagMethodCombo->addItem(tr("Automatic time lag optimization"))',
            'timeLagMethodCombo->addItem(tr("Pre-whitening block-bootstrap (Vitale et al. 2024)"))',
        ]
        positions = [source.index(label) for label in labels]
        self.assertEqual(positions, sorted(positions))
        self.assertIn("timeLagMethodCombo->setItemData(4", source)

    def test_settings_button_branches_between_existing_and_pwb_dialogs(self):
        source = read("src/advprocessingoptions.cpp")
        self.assertIn("tlSettingsButton->setEnabled(n == 3 || n == 4);", source)
        self.assertIn(
            "tlSettingsButton->setEnabled(ecProject_->screenTlagMeth() == 4 || ecProject_->screenTlagMeth() == 5);",
            source,
        )
        self.assertIn("if (ecProject_->screenTlagMeth() == 5)", source)
        self.assertIn("pwbTlDialog_->refresh();", source)
        self.assertIn("tlDialog_->refresh();", source)

    def test_pwb_settings_have_state_save_load_tags_and_source_entries(self):
        state = read("src/ecprojectstate.h")
        project = read("src/ecproject.cpp")
        defs = read("src/ecinidefs.h")
        sources = read("sources.pri")
        self.assertIn("struct PwbTimelagState", state)
        self.assertIn("PwbTimelagState pwbTimelag", state)
        self.assertIn("INIGROUP_PWB_TIMELAG", defs)
        for idx in range(16):
            self.assertIn(f"INI_PWB_TIMELAG_{idx}", defs)
            self.assertIn(f"INI_PWB_TIMELAG_{idx}", project)
        self.assertIn("project_ini.beginGroup(EcIni::INIGROUP_PWB_TIMELAG)", project)
        self.assertIn("pwbTimelag.co2_min_lag", project)
        self.assertIn("src/pwbtimelagsettingsdialog.h", sources)
        self.assertIn("src/pwbtimelagsettingsdialog.cpp", sources)

    def test_pwb_dialog_exposes_all_requested_controls(self):
        dialog = read("src/pwbtimelagsettingsdialog.cpp")
        for text in (
            "Time lag search windows",
            # The four fixed gas labels are gone: rows are generated from the
            # project's gas records, so a site can carry the same species on
            # more than one analyser.
            "rebuildLagRows",
            "ecProject_->gasColumns()",
            "Bootstrap replicates",
            "Block length",
            "Minimum valid fraction",
            "HDI threshold",
            "Deviation threshold",
            "HDI prefilter",
            "Smoothing width",
            "Random seed",
        ):
            self.assertIn(text, dialog)
        # The per-gas search windows are stored on the gas records now, not on
        # the four flat pwb_<gas>_lag keys, so setPwbCo2MinLag and its seven
        # siblings are gone. The remaining setters are the PWB algorithm
        # parameters, which are one per project and stay flat.
        for gas_field in ("proc.pwbMinLag", "proc.pwbMaxLag"):
            self.assertIn(gas_field, dialog)
        for setter in (
            "setPwbNBootstrap",
            "setPwbBlockLength",
            "setPwbMinValidFrac",
            "setPwbHdiThresh",
            "setPwbDevThresh",
            "setPwbHdiPrefilter",
            "setPwbSmoothingWidth",
            "setPwbRandomSeed",
        ):
            self.assertIn(setter, dialog)

    def test_pwb_dialog_accepts_only_the_half_hourly_table(self):
        # The *_pwb_timelag_opt_* summary written beside the table, or any
        # time-lag optimisation file, used to be accepted and silently turned
        # the run into covariance maximisation. The engine stops on one now,
        # and the dialog refuses it rather than handing it on.
        dialog = read("src/pwbtimelagsettingsdialog.cpp")
        self.assertNotIn("AncillaryFileTest", dialog)
        self.assertNotIn("aggregate time-lag file", dialog)
        self.assertIn("This file is not a PWB half-hourly time-lag table.", dialog)
        self.assertIn("written by an earlier version", dialog)
        self.assertIn("fileBrowse->clear();", dialog)
        accepted = re.findall(r'firstLine == QLatin1String\("(PWB_TIMELAG_CACHE_VERSION=\d+)"\)', dialog)
        self.assertEqual(len(accepted), 1, accepted)

        # The engine reads exactly one version; the dialog must accept that one.
        engine = ROOT.parent / "eddyflow-engine" / "src" / "src_rp" / "pwb_timelag_handle.f90"
        if not engine.exists():
            self.skipTest("eddyflow-engine is not checked out beside this repository")
        read_versions = set(re.findall(r"trim\(line\) /= '(PWB_TIMELAG_CACHE_VERSION=\d+)'",
                                       engine.read_text(encoding="utf-8")))
        self.assertEqual(read_versions, {accepted[0]})


if __name__ == "__main__":
    unittest.main()
