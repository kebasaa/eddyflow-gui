"""File > Export > EddyPro Project, and what keeps it honest.

The export is the lossy direction. This program measures more gases, knows
more instruments and offers more methods than EddyPro 7, and the export
replaces everything EddyPro cannot express rather than refusing. That only
works if three things hold:

1. **Every instrument has an answer.** A model EddyPro does not know is
   written as its generic sonic or analyser. The open/closed-path choice for
   an analyser decides which generic it gets, so an analyser added to the
   metadata vocabulary without being classified here fails this test rather
   than silently falling back on its tube.
2. **Every value EddyPro would misread is replaced.** EddyPro reads most
   method keys by their first character and sends unknown values to a
   default branch - tlag_meth 5 becomes no time lag compensation at all,
   despike_vm 2 becomes Mauder 2013. Each such value has a rule.
3. **The fourth gas keeps its constants.** EddyPro reads gas_mw <= 0 as
   "use N2O's", so a COS column exported with this program's "species
   default" sentinel would be computed as nitrous oxide.

The EddyPro vocabulary pinned below is copied from LI-COR's eddypro-gui
(src/dlproject.cpp), the only authority on what EddyPro accepts.

SmartFlux packaging uses the same project writer and must not change: it is
pinned to the two-argument call.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
SRC = GUI_ROOT / "src"
EXPORT_H = SRC / "eddyproexport.h"
EXPORT_CPP = SRC / "eddyproexport.cpp"
DIALOG_CPP = SRC / "eddyprogasslotdialog.cpp"
MAINWINDOW_CPP = SRC / "mainwindow.cpp"
MAINWINDOW_H = SRC / "mainwindow.h"
DLPROJECT_CPP = SRC / "dlproject.cpp"
ECPROJECT_CPP = SRC / "ecproject.cpp"
SMARTFLUX_BAR = SRC / "smartfluxbar.cpp"
SOURCES_PRI = GUI_ROOT / "sources.pri"

#: eddypro-gui src/dlproject.cpp, ANEM_MODEL / IRGA_MODEL strings.
UPSTREAM_SONICS = {
    "csat3", "hs_50", "hs_100", "r2", "r3_50", "r3_100", "r3a_100", "wm",
    "wmpro", "usa1_standard", "usa1_fast", "81000", "generic_sonic", "csat3b",
    "usoni3_classa_mp", "usoni3_cage_mp", "81000v", "81000re", "81000vre",
}
UPSTREAM_IRGAS = {
    "li6262", "li7000", "li7500", "li7500a", "li7200", "li7700",
    "generic_open_path", "generic_closed_path", "open_path_krypton",
    "open_path_lyman", "closed_path_krypton", "closed_path_lyman",
    "li7500rs", "li7200rs", "li7500ds",
}


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def function_body(src, signature):
    start = src.index(signature)
    brace = src.index("{", start)
    depth = 0
    for i in range(brace, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[brace:i + 1]
    raise AssertionError("unbalanced braces after " + signature)


def string_set(body):
    return set(re.findall(r'QStringLiteral\("([^"]+)"\)', body))


def call_arguments(src, name):
    """The top-level arguments of the first call to `name` in `src`."""
    start = src.index(name + "(") + len(name) + 1
    depth, args, current = 1, [], ""
    for ch in src[start:]:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
            if depth == 0:
                args.append(current.strip())
                return args
        if ch == "," and depth == 1:
            args.append(current.strip())
            current = ""
            continue
        current += ch
    raise AssertionError("unterminated call to " + name)


class MenuTest(unittest.TestCase):
    def setUp(self):
        self.mw = read(MAINWINDOW_CPP)

    def test_imports_live_in_an_import_submenu(self):
        self.assertIn('fileMenuImport = fileMenu->addMenu(tr("&Import"));', self.mw)
        self.assertIn("fileMenuImport->addAction(importEddyProAction);", self.mw)
        self.assertIn("fileMenuImport->addAction(importEddyUhAction);", self.mw)
        self.assertNotIn("fileMenu->addAction(importEddyProAction);", self.mw)
        self.assertNotIn("fileMenu->addAction(importEddyUhAction);", self.mw)

    def test_export_lives_in_an_export_submenu(self):
        self.assertIn('fileMenuExport = fileMenu->addMenu(tr("&Export"));', self.mw)
        self.assertIn("fileMenuExport->addAction(exportEddyProAction);", self.mw)
        self.assertIn("connect(exportEddyProAction, &QAction::triggered", self.mw)
        self.assertIn("void exportEddyProFile();", read(MAINWINDOW_H))

    def test_export_is_enabled_exactly_where_save_as_is(self):
        save_as = re.findall(r"saveAsAction->setEnabled\(([^)]*)\);", self.mw)
        export = re.findall(r"exportEddyProAction->setEnabled\(([^)]*)\);", self.mw)
        # One extra: the action starts disabled, before any project is open.
        self.assertEqual(export[0], "false")
        self.assertEqual(save_as, export[1:])

    def test_the_gas_columns_are_asked_before_the_file(self):
        body = function_body(self.mw, "void MainWindow::exportEddyProFile()")
        self.assertLess(body.index("EddyProGasSlotDialog"),
                        body.index("QFileDialog::getSaveFileName"))

    def test_new_files_are_built(self):
        pri = read(SOURCES_PRI)
        for name in ("eddyproexport", "eddyprogasslotdialog"):
            self.assertIn("src/%s.h" % name, pri)
            self.assertIn("src/%s.cpp" % name, pri)


class VocabularyTest(unittest.TestCase):
    def setUp(self):
        self.cpp = read(EXPORT_CPP)
        self.dl = read(DLPROJECT_CPP)

    def test_upstream_sonics_are_pinned(self):
        body = function_body(self.cpp, "const QSet<QString>& eddyProSonicModels()")
        self.assertEqual(string_set(body), UPSTREAM_SONICS)

    def test_upstream_analysers_are_pinned(self):
        body = function_body(self.cpp, "const QSet<QString>& eddyProIrgaModels()")
        self.assertEqual(string_set(body), UPSTREAM_IRGAS)

    def test_every_analyser_of_ours_is_classified(self):
        ours = set(re.findall(
            r'IRGA_MODEL_STRING_\d+ = QStringLiteral\("([^"]+)"\)', self.dl))
        self.assertTrue(ours, "no analyser models found in dlproject.cpp")
        open_path = string_set(function_body(self.cpp, "const QSet<QString>& openPathIrgas()"))
        closed_path = string_set(function_body(self.cpp, "const QSet<QString>& closedPathIrgas()"))
        self.assertFalse(open_path & closed_path)
        unclassified = ours - UPSTREAM_IRGAS - open_path - closed_path
        self.assertEqual(unclassified, set(),
                         "classify these as open- or closed-path for the export")

    def test_campbell_sonics_eddypro_knows_are_renamed_not_replaced(self):
        body = function_body(self.cpp, "const QHash<QString, QString>& renamedSonics()")
        self.assertIn('{ QStringLiteral("csi_csat3"),  QStringLiteral("csat3") }', body)
        self.assertIn('{ QStringLiteral("csi_csat3b"), QStringLiteral("csat3b") }', body)

    def test_generic_instruments_carry_eddypros_manufacturers(self):
        self.assertIn('kOtherSonic = QStringLiteral("other_sonic")', self.cpp)
        self.assertIn('kOtherIrga = QStringLiteral("other_irga")', self.cpp)

    def test_renamed_instruments_follow_into_columns_and_master_sonic(self):
        self.assertIn("renamed.contains(instrument)", self.cpp)
        self.assertIn("out.setValue(EcIni::INI_PROJECT_17, QString(standIn", self.cpp)


class OptionRulesTest(unittest.TestCase):
    def setUp(self):
        self.cpp = read(EXPORT_CPP)
        self.rules = function_body(self.cpp, "const QVector<OptionRule>& optionRules()")

    def assert_replaced(self, key_const, unsupported, target):
        pattern = (re.escape(key_const) + r",.*?\{ " + re.escape(unsupported)
                   + r" \}, " + str(target) + r", false[ ,]")
        self.assertRegex(self.rules, re.compile(pattern, re.S))

    def test_values_eddypro_would_misread_are_replaced_by_its_default(self):
        # tlag_meth PWB -> automatic time lag optimization, the closest method
        self.assert_replaced("EcIni::INI_SCREEN_SETTINGS_7", "5", 4)
        self.assert_replaced("EcIni::INI_PROJECT_46", "7", 1)          # hf_meth Eugster-Senn
        self.assert_replaced("EcIni::INI_PROJECT_55", "4", 1)          # qc_meth Vitale
        self.assert_replaced("EcIni::INI_RAND_ERROR_0", "4, 5", 0)     # ru_meth
        self.assert_replaced("EcIni::INI_SCREEN_PARAM_60", "2", 0)     # despike_vm

    def test_every_option_of_ours_is_removed(self):
        reported = [
            "INI_PROJECT_83", "INI_PROJECT_84", "INI_PROJECT_87",
            "INI_RAND_ERROR_5", "INI_RAND_ERROR_6",
            "INI_SCREEN_SETTINGS_102", "INI_SCREEN_SETTINGS_105",
            "INI_SCREEN_SETTINGS_107", "INI_SCREEN_SETTINGS_108",
            "INI_SCREEN_SETTINGS_112", "INI_SCREEN_SETTINGS_118",
            "INI_SCREEN_SETTINGS_120", "INI_SCREEN_TESTS_9",
        ]
        for const in reported:
            self.assertRegex(self.rules,
                             re.compile(r"EcIni::" + const + r"\b.*?, true \}", re.S),
                             const)
        silent = function_body(
            self.cpp, "const QVector<QPair<QString, QString>>& silentRemovals()")
        for const in ["INI_PROJECT_85", "INI_PROJECT_86", "INI_RAND_ERROR_4",
                      "INI_SCREEN_SETTINGS_103", "INI_SCREEN_SETTINGS_104",
                      "INI_SCREEN_SETTINGS_106", "INI_SCREEN_SETTINGS_109",
                      "INI_SCREEN_SETTINGS_110", "INI_SCREEN_SETTINGS_111",
                      "INI_SCREEN_SETTINGS_113", "INI_SCREEN_SETTINGS_114",
                      "INI_SCREEN_SETTINGS_115", "INI_SCREEN_SETTINGS_116",
                      "INI_SCREEN_SETTINGS_117", "INI_SCREEN_SETTINGS_119",
                      "INI_SCREEN_SETTINGS_121", "INI_SCREEN_SETTINGS_122",
                      "INI_SCREEN_SETTINGS_123", "INI_SCREEN_SETTINGS_124",
                      "INI_SCREEN_PARAM_61", "INI_SCREEN_PARAM_62",
                      "INI_SCREEN_PARAM_63", "INI_SCREEN_PARAM_64",
                      "INI_BIOMET_RH_OVERRIDE"]:
            self.assertIn("EcIni::" + const + " }", silent, const)

    def test_signal_strength_and_instrument_gap_limits_are_removed(self):
        self.assertIn('k.startsWith(QLatin1String("agc_"))', self.cpp)
        self.assertIn("EcIni::iniScreenSettingsInstrMaxLack(", self.cpp)

    def test_a_removed_option_is_reported_only_when_it_was_on(self):
        self.assertIn("if (value != rule.target)", self.cpp)


class FilesAndGasesTest(unittest.TestCase):
    def setUp(self):
        self.cpp = read(EXPORT_CPP)

    def test_file_names(self):
        self.assertIn('QStringLiteral("_EP.") + Defs::METADATA_FILE_EXT', self.cpp)
        self.assertIn('QStringLiteral("_EP_export_report.txt")', self.cpp)

    def test_the_project_points_at_the_new_metadata(self):
        self.assertIn("out.setValue(EcIni::INI_PROJECT_9, 1);", self.cpp)
        self.assertIn("out.setValue(EcIni::INI_PROJECT_10, "
                      "QDir::fromNativeSeparators(metadataFile_));", self.cpp)

    def test_the_fourth_gas_carries_its_own_constants(self):
        body = function_body(self.cpp, "bool EddyProExport::convertProjectOptions(")
        self.assertIn("if (mw <= 0.0)", body)
        self.assertIn("mw = entry->molecularWeight;", body)
        self.assertIn("diff = entry->diffusivity;", body)
        # ... and the user is told EddyPro's interface would undo that.
        self.assertIn("EddyPro's interface resets both", body)

    def test_an_unknown_fourth_gas_is_written_as_n2o(self):
        self.assertIn('newVar = QStringLiteral("n2o");', self.cpp)

    def test_pmol_is_rescaled_to_ppb(self):
        self.assertIn("a *= 0.001;", self.cpp)
        self.assertIn("b *= 0.001;", self.cpp)

    def test_the_dialog_offers_none_for_every_slot(self):
        dialog = read(DIALOG_CPP)
        self.assertIn('combo_[s]->addItem(tr("None"), 0);', dialog)


class WriterTest(unittest.TestCase):
    def test_a_record_without_a_column_takes_no_slot(self):
        body = function_body(read(ECPROJECT_CPP),
                             "void EcProject::writeEddyProCompatibleKeys(")
        self.assertIn("if (gases.at(i).rawColumn <= 0) { continue; }", body)
        self.assertIn("ini.setValue(QLatin1String(colKey[i]), slotCol[i]);", body)

    def test_smartflux_still_takes_the_first_match(self):
        args = call_arguments(read(SMARTFLUX_BAR), "exportEddyProProject")
        self.assertEqual(len(args), 2, args)


if __name__ == "__main__":
    unittest.main()
