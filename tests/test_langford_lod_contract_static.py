"""Limit of detection after Langford et al. (2015): the interface and the engine agree.

The method is offered in two places - as random uncertainty method
``ru_meth = 6`` and as flux detection limit ``detlim_meth = 2`` - and both read
one set of settings from one dialog. What could break silently:

**A label that loses its citation.** Each lag window names the paper it comes
from: Langford et al. (2015), Maier et al. (2022), and the automatic one after
Spirig et al. (2005), whose rule is EddyFlow's own and says so.

**A key the engine never sees.** ``lod_window``, ``lod_stat`` and
``lod_its_mult`` go to ``[RawProcess_Settings]``, which only RP reads;
``lod_alpha`` to ``[Project]``, because FCC writes the LoD columns too.

**A default that drifts.** +/-150-180 s, RMSE, ten integral scales and
alpha = 1.96 on both sides, or the same file computes different limits
depending on whether it was opened here first.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_RP_TAGS = ENGINE_ROOT / "src" / "src_rp" / "m_rp_global_var.f90"
ENGINE_PRJ_TAGS = ENGINE_ROOT / "src" / "src_common" / "m_common_global_var.f90"
ENGINE_READER = ENGINE_ROOT / "src" / "src_rp" / "read_ini_rp.f90"
ENGINE_DECODER = ENGINE_ROOT / "src" / "src_common" / "write_processing_project_variables.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


SRC = GUI_ROOT / "src"
DIALOG = read(SRC / "langfordsettingsdialog.cpp")
DETLIM = read(SRC / "detlimsettingsdialog.cpp")
DETLIM_H = read(SRC / "detlimsettingsdialog.h")
PAGE = read(SRC / "advstatisticaloptions.cpp")
INIDEFS = read(SRC / "ecinidefs.h")
STATE = read(SRC / "ecprojectstate.h")
PROJECT = read(SRC / "ecproject.cpp")
SOURCES = read(GUI_ROOT / "sources.pri")


class TheLabelsCiteTheirPapers(unittest.TestCase):

    def test_the_random_uncertainty_row(self):
        self.assertIn('randomMethodCombo->addItem(tr("Limit of detection (Langford et al. 2015)"));', PAGE)
        self.assertIn("randomMethodCombo->setItemData(4, 6);", PAGE)

    def test_the_detection_limit_row(self):
        self.assertIn('methodCombo->addItem(tr("Langford et al. (2015)"));', DETLIM)

    def test_the_dialog_title(self):
        self.assertIn('setWindowTitle(tr("Limit of detection (Langford et al. 2015)"));', DIALOG)

    def test_the_three_windows_in_ini_order(self):
        labels = re.findall(r'windowCombo->addItem\(tr\("([^"]*)"\)\);', DIALOG)
        self.assertEqual(labels, [
            "±150–180 s (Langford et al. 2015)",
            "±160–180 s (Maier et al. 2022)",
            "Automatic (multiple of the integral turbulence scale, after Spirig et al. 2005)",
        ])

    def test_the_automatic_rule_is_owned_as_ours(self):
        m = re.search(r"windowCombo->setItemData\(2, tr\((.*?)\), Qt::ToolTipRole\);", DIALOG, re.S)
        self.assertIsNotNone(m)
        tip = "".join(re.findall(r'"([^"]*)"', m.group(1)))
        self.assertIn("the rule here is EddyFlow's own", tip)
        self.assertIn("Spirig et al. (2005)", tip)


class OneDialogForBothUses(unittest.TestCase):

    def test_both_buttons_open_the_same_dialog(self):
        self.assertIn("void langfordSettingsRequested();", DETLIM_H)
        self.assertIn("connect(detlimDialog_, &DetlimSettingsDialog::langfordSettingsRequested,", PAGE)
        self.assertIn("this, &AdvStatisticalOptions::showLangfordSettingsDialog);", PAGE)
        self.assertEqual(PAGE.count("new LangfordSettingsDialog("), 1)

    def test_the_detlim_row_is_the_stored_value(self):
        self.assertIn("ecProject_->setScreenDetlimMethod(index);", DETLIM)
        self.assertIn("methodCombo->setCurrentIndex((method >= 0 && method <= 2) ? method : 0);", DETLIM)

    def test_it_is_built(self):
        self.assertIn("src/langfordsettingsdialog.cpp", SOURCES)
        self.assertIn("src/langfordsettingsdialog.h", SOURCES)


class TheKeysReachTheEngine(unittest.TestCase):

    def test_the_ini_names(self):
        for const, key in (("INI_SCREEN_SETTINGS_121", "lod_window"),
                           ("INI_SCREEN_SETTINGS_122", "lod_stat"),
                           ("INI_SCREEN_SETTINGS_123", "lod_its_mult"),
                           ("INI_RAND_ERROR_4", "lod_alpha")):
            self.assertRegex(INIDEFS, r'%s\s*= QStringLiteral\("%s"\)' % (const, key))

    def test_each_is_saved_loaded_and_reset_for_a_new_project(self):
        for field in ("screenSetting.lod_window", "screenSetting.lod_stat",
                      "screenSetting.lod_its_mult", "randomError.lod_alpha"):
            # newEcProject, load, save and the comparison each name it.
            self.assertGreaterEqual(PROJECT.count(field), 4, field)

    @unittest.skipUnless(ENGINE_RP_TAGS.is_file(), "engine repo not beside the GUI")
    def test_the_engine_declares_them(self):
        rp = read(ENGINE_RP_TAGS)
        prj = read(ENGINE_PRJ_TAGS)
        for key in ("lod_window", "lod_stat", "lod_its_mult"):
            self.assertIn("'%s'" % key, rp)
        self.assertIn("'lod_alpha'", prj)


class TheDefaultsAgree(unittest.TestCase):

    def test_the_interface_defaults(self):
        self.assertIn("int lod_window = 0;", STATE)
        self.assertIn("int lod_stat = 0;", STATE)
        self.assertIn("qreal lod_its_mult = 10.0;", STATE)
        self.assertIn("qreal lod_alpha = 1.96;", STATE)
        for line in ("constexpr int DefaultWindow = 0;", "constexpr int DefaultStat = 0;",
                     "constexpr double DefaultItsMult = 10.0;", "constexpr double DefaultAlpha = 1.96;"):
            self.assertIn(line, DIALOG)

    @unittest.skipUnless(ENGINE_READER.is_file(), "engine repo not beside the GUI")
    def test_the_engine_defaults(self):
        reader = read(ENGINE_READER)
        decoder = read(ENGINE_DECODER)
        self.assertIn("RUsetup%lod_window = 0", reader)
        self.assertIn("RUsetup%lod_stat = 'rmse'", reader)
        self.assertIn("RUsetup%lod_its_mult = 10d0", reader)
        self.assertIn("RUsetup%lod_alpha = 1.96d0", decoder)
        self.assertIn("RUsetup%meth = 'langford_15'", decoder)
        self.assertIn("RPSetup%detlim_meth = 'langford_15'", reader)


class TheSpectralCorrectionOfTheErrors(unittest.TestCase):
    """ru_spec_corr: off by default on both sides, also for a file without it,
    because EddyPro does not do it."""

    def test_the_key_and_its_default(self):
        self.assertRegex(INIDEFS, r'INI_RAND_ERROR_5\s*= QStringLiteral\("ru_spec_corr"\)')
        self.assertIn("int spec_corr = 0;", STATE)
        self.assertGreaterEqual(PROJECT.count("randomError.spec_corr"), 4)

    def test_the_tooltip_says_what_and_why(self):
        m = re.search(r"specCorrCheckBox->setToolTip\(tr\((.*?)\)\);", PAGE, re.S)
        self.assertIsNotNone(m)
        tip = "".join(re.findall(r'"([^"]*)"', m.group(1)))
        self.assertIn("Langford et al. (2015)", tip)
        self.assertIn("Off by default", tip)
        self.assertIn("EddyPro", tip)
        self.assertIn("spectral correction factor", tip)

    @unittest.skipUnless(ENGINE_DECODER.is_file(), "engine repo not beside the GUI")
    def test_the_engine_default(self):
        decoder = read(ENGINE_DECODER)
        self.assertIn("RUsetup%spec_corr = .false.", decoder)
        self.assertIn("'ru_spec_corr'", read(ENGINE_PRJ_TAGS))


if __name__ == "__main__":
    unittest.main()
