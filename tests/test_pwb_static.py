from pathlib import Path
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
            "Defs::CO2_STRING",
            "Defs::H2O_STRING",
            "Defs::CH4_STRING",
            "Defs::GAS4_STRING",
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
        for setter in (
            "setPwbCo2MinLag",
            "setPwbCo2MaxLag",
            "setPwbH2oMinLag",
            "setPwbH2oMaxLag",
            "setPwbCh4MinLag",
            "setPwbCh4MaxLag",
            "setPwbGas4MinLag",
            "setPwbGas4MaxLag",
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


if __name__ == "__main__":
    unittest.main()
