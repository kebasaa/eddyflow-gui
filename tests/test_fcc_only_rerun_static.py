"""Re-running only the flux correction of an Advanced run.

An Advanced run is two passes: raw processing (eddyflow_rp), then flux
computation and correction (eddyflow_fcc). FCC reads only [Project] and the
[FluxCorrection*] sections, and takes everything raw processing computed from
one FLUXNET file (ex_file) and the cospectra folders. When those are still in
the output folder and no setting raw processing reads has changed, the
interface offers to run FCC alone (src/previousrun.cpp), in the dates dialog
an Advanced run always shows.

Pinned here: the "keep" switch reaches the engine and stays out of SmartFlux
packages; the engine still reads it and still deletes the file without it;
the comparison looks at exactly what raw processing reads; the names the
scanner looks for are the engine's; deleting is never the default; and the
correction-only start goes through the existing FCC launch.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE = GUI_ROOT.parent / "eddyflow-engine" / "src"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def body(src, signature):
    start = src.index(signature)
    end = src.index("\n}\n", start)
    return src[start:end]


DEFS = read(GUI_ROOT / "src" / "defs.h")
INIDEFS = read(GUI_ROOT / "src" / "ecinidefs.h")
STATE = read(GUI_ROOT / "src" / "ecprojectstate.h")
PROJECT = read(GUI_ROOT / "src" / "ecproject.cpp")
OUTPUTS = read(GUI_ROOT / "src" / "advoutputoptions.cpp")
SCANNER = read(GUI_ROOT / "src" / "previousrun.cpp")
DIALOG = read(GUI_ROOT / "src" / "detectdaterangedialog.cpp")
MAIN = read(GUI_ROOT / "src" / "mainwindow.cpp")
RUNPAGE = read(GUI_ROOT / "src" / "runpage.cpp")
SOURCES = read(GUI_ROOT / "CMakeLists.txt")


def engine(relative):
    path = ENGINE / relative
    if not path.exists():
        raise unittest.SkipTest("eddyflow-engine is not checked out beside the GUI")
    return read(path)


class TheKeepSwitch(unittest.TestCase):

    def test_the_key_is_named(self):
        self.assertIn('INI_SPEC_SETTINGS_54    = QStringLiteral("keep_parent_fluxnet_file")',
                      INIDEFS)

    def test_it_is_on_by_default(self):
        """A project from before the key keeps its results too."""
        self.assertIn("int keep_parent_fluxnet_file = 1;", STATE)

    def test_it_is_saved_and_loaded(self):
        self.assertIn("project_ini.setValue(EcIni::INI_SPEC_SETTINGS_54, "
                      "ec_project_state_.spectraSettings.keep_parent_fluxnet_file);",
                      PROJECT)
        self.assertIn("project_ini.value(EcIni::INI_SPEC_SETTINGS_54,", PROJECT)

    def test_smartflux_packages_leave_it_out(self):
        """EddyPro 7.0.9 does not write it; without it the engine deletes."""
        exporter = body(PROJECT, "void EcProject::writeEddyProCompatibleKeys(")
        self.assertIn("ini.remove(EcIni::INI_SPEC_SETTINGS_54);", exporter)

    def test_the_box_sets_it(self):
        self.assertIn("ecProject_->setSpectraKeepParentFluxnetFile(checked);", OUTPUTS)
        self.assertIn("keepRawResultsCheckBox->setChecked("
                      "ecProject_->spectraKeepParentFluxnetFile());", OUTPUTS)

    def test_the_box_sits_above_the_assessment_outputs(self):
        layout = OUTPUTS.index("keepRawResultsLayout->addWidget(keepRawResultsCheckBox);")
        title = OUTPUTS.index("outputLayout->addWidget(titlePreprocessing, 4, 0);")
        self.assertLess(layout, title)

    def test_the_engine_reads_it_and_deletes_without_it(self):
        labels = engine("src_fcc/m_fx_global_var_mod.f90")
        self.assertIn("'keep_parent_fluxnet_file'", labels)
        main = engine("src_fcc/eddyflow-fcc_main.f90")
        self.assertIn("if (.not. FCCsetup%keep_parent) then", main)


class TheComparisonIsWhatRawProcessingReads(unittest.TestCase):

    def test_raw_processing_reads_project_and_rawprocess_sections(self):
        main = engine("src_rp/eddyflow-rp_main.f90")
        self.assertIn("call ReadIniRP('RawProcess')", main)
        ini = engine("src_rp/read_ini_rp.f90")
        self.assertIn("call ParseIniFile(PrjPath, 'Project',", ini)
        self.assertIn('const QString RawProcessPrefix = QStringLiteral("RawProcess");',
                      SCANNER)

    def test_the_spectral_method_is_not_correction_only(self):
        """RP writes it into the ex file and FCC copies that field as it is."""
        allowed = body(SCANNER, "const QSet<QString>& correctionOnlyKeys()")
        self.assertNotIn('"hf_meth"', allowed)
        self.assertIn("Copy TIMELAG_DETECTION_METHOD thru SPECTRAL_CORRECTION_METHOD",
                      engine("src_common/read_ex_record.f90"))

    def test_the_random_error_and_detection_limit_are_not_correction_only(self):
        allowed = body(SCANNER, "const QSet<QString>& correctionOnlyKeys()")
        for key in ("ru_meth", "lod_meth", "wpl_meth", "err_label", "make_dataset"):
            self.assertNotIn('"%s"' % key, allowed)

    def test_full_output_and_metadata_are_off_in_rp_when_fcc_follows(self):
        """Why out_rich and out_metadata may change."""
        main = engine("src_rp/eddyflow-rp_main.f90")
        self.assertRegex(main, r"EddyFlowProj%out_full\s+= \.false\.")
        self.assertRegex(main, r"EddyFlowProj%out_md\s+= \.false\.")

    def test_the_header_markers_are_fccs(self):
        judge = engine("src_fcc/eddyflow-fcc_main.f90")
        judge = judge[judge.index("subroutine JudgeExHeader"):]
        for marker in ("NUM_WATER_FLUX", "H2O_BIOMET_MOLE_FRACTION", "CEC_METH",
                       "CEC_NS_", "_MOIST_SLOT", "_MOIST_REF"):
            self.assertIn("'%s'" % marker, judge)
            self.assertIn('"%s"' % marker, SCANNER)


class TheNamesAreTheEngines(unittest.TestCase):

    def test_the_cospectra_folders(self):
        common = engine("src_common/m_common_global_var.f90")
        self.assertIn("SubDirBinCospectra      = 'eddyflow_binned_cospectra'", common)
        self.assertIn("SubDirCospectra         = 'eddyflow_full_cospectra'", common)
        self.assertIn('OUT_BINNED_COSPECTRA_DIR    = QStringLiteral("/eddyflow_binned_cospectra")',
                      DEFS)
        self.assertIn('OUT_FULL_COSPECTRA_DIR      = QStringLiteral("/eddyflow_full_cospectra")',
                      DEFS)
        self.assertNotIn("EddyFlow_full_cospectra", MAIN)

    def test_every_sub_folder_the_scanner_lists_is_one(self):
        engine_text = (engine("src_common/m_common_global_var.f90")
                       + engine("src_rp/m_rp_global_var.f90")
                       + engine("src_fcc/m_fx_global_var_mod.f90"))
        folders = body(SCANNER, "const QStringList& PreviousRun::engineSubFolders()")
        names = re.findall(r'QStringLiteral\("(eddyflow_[a-z_]+)"\)', folders)
        self.assertEqual(len(names), 7)
        for name in names:
            self.assertIn("'%s'" % name, engine_text)

    def test_the_run_tag(self):
        """_<start>_<mode>, init_env.f90 and tag_run_mode.f90."""
        modes = engine("src_common/tag_run_mode.f90")
        for mode in ("_exp", "_adv", "_mdr", "_nul"):
            self.assertIn("'%s'" % mode, modes)
        self.assertIn("(?:adv|exp|mdr|nul)", SCANNER)

    def test_the_record_is_where_fcc_writes_its_own(self):
        main = engine("src_fcc/eddyflow-fcc_main.f90")
        self.assertIn("trim(adjustl(Dir%main_out)) // 'processing' &", main)
        self.assertIn('QStringLiteral("/processing_") + tag', SCANNER)


class TheDialogAsks(unittest.TestCase):

    def test_keep_and_run_is_the_default(self):
        self.assertIn("keepAndRunButton->setDefault(true);", DIALOG)
        self.assertIn("deleteAndRunButton_->setDefault(false);", DIALOG)
        self.assertIn("deleteAndRunButton_->setAutoDefault(false);", DIALOG)

    def test_the_labels(self):
        for text in ('tr("Delete and run")', 'tr("Keep and run")', 'tr("Cancel")',
                     'tr("Full re-run (raw processing and flux "'):
            self.assertIn(text, DIALOG)

    def test_an_advanced_run_shows_it_when_there_was_a_run(self):
        fn = body(MAIN, "bool MainWindow::getDatesRangeDialog(Defs::CurrRunMode mode)")
        self.assertIn("return showDatesRangeDialog(mode, isSubperiodSet);", fn)
        fn = body(MAIN, "bool MainWindow::showDatesRangeDialog(Defs::CurrRunMode mode, bool withDates)")
        self.assertIn("const bool askPreviousRun = advanced && scan.found();", fn)
        self.assertIn("if (askPreviousRun) { detectDateRangeDialog.showPreviousRun(scan); }", fn)

    def test_an_empty_output_folder_goes_straight_to_the_run(self):
        """Nothing to keep or delete and no dates to check: no dialog."""
        fn = body(MAIN, "bool MainWindow::showDatesRangeDialog(Defs::CurrRunMode mode, bool withDates)")
        skip = fn.index("if (!showDates && !askPreviousRun) { return true; }")
        self.assertLess(fn.index("scan = scanPreviousRun(&cancelled);"), skip)
        self.assertLess(skip, fn.index("DetectDateRangeDialog detectDateRangeDialog("))


class TheRunStartsWhereItAlwaysDid(unittest.TestCase):

    def test_correction_only_answers_no(self):
        """No is the existing "reuse a previous run" answer: runAdvancedStep_1
        jumps to runAdvancedStep_2 and does not count raw datasets."""
        fn = body(MAIN, "int MainWindow::testBeforeRunningPassed(int step)")
        self.assertIn("if (step == 0 && advancedClicked_ && pendingRerun_.active)", fn)
        self.assertIn("returnValue = QMessageBox::No;", fn)
        self.assertIn("ecProject_->setModified(true);", fn)
        self.assertNotIn("spectraExDir", fn)

    def test_the_old_essentials_search_is_gone(self):
        self.assertNotIn("testForPreviousData", MAIN)
        self.assertNotIn("Essentials file path:", RUNPAGE)

    def test_deleting_comes_before_the_disk_check(self):
        fn = MAIN[MAIN.index("void MainWindow::runAdvancedStep_1()"):]
        self.assertLess(fn.index("testBeforeRunningPassed(0)"),
                        fn.index("confirmOutputSpace(ret == QMessageBox::Yes)"))

    def test_the_pass_is_settled_when_the_engine_exits(self):
        fn = body(MAIN, "void MainWindow::displayExitDialog()")
        self.assertLess(fn.index("runPage()->stopRun();"),
                        fn.index("finishCorrectionPass("))

    def test_a_correction_only_start_has_a_log_of_its_own(self):
        start = RUNPAGE.index("case Defs::CurrRunStatus::Advanced_FCC:")
        case = RUNPAGE[start:RUNPAGE.index("break;", start)]
        self.assertIn("if (continuing)", case)
        self.assertIn("errorEdit_->clear();", case)

    def test_scanning_and_deleting_keep_the_window_alive(self):
        """Tens of thousands of cospectra files on a network share take
        seconds to list and minutes to delete; both run in a worker thread
        behind a progress dialog that can stop them."""
        scan = body(MAIN, "PreviousRun::Scan MainWindow::scanPreviousRun(bool* cancelled)")
        self.assertIn("QtConcurrent::run(", scan)
        self.assertIn("PreviousRun::scan(copy, fccNeeded, &progress)", scan)
        self.assertIn("progress.cancelled = true;", scan)
        delete = body(MAIN, "bool MainWindow::deleteObsoleteOutputs(")
        self.assertIn("QtConcurrent::run(", delete)
        self.assertIn("if (stop) { break; }", delete)
        self.assertIn("dialog->setValue(n);", delete)
        self.assertIn("if (progress->cancelled) { break; }", SCANNER)

    def test_a_cancelled_scan_starts_nothing(self):
        fn = body(MAIN, "bool MainWindow::showDatesRangeDialog(Defs::CurrRunMode mode, bool withDates)")
        self.assertIn("scan = scanPreviousRun(&cancelled);", fn)
        self.assertLess(fn.index("if (cancelled)"), fn.index("showPreviousRun(scan)"))

    def test_the_scan_is_not_quadratic(self):
        """add() once checked paths.contains() for every file."""
        fn = body(SCANNER, "void add(PreviousRun::Files& files, const Entry& e)")
        self.assertNotIn("contains(", fn)

    def test_the_files_are_built(self):
        self.assertIn("src/previousrun.h", SOURCES)
        self.assertIn("src/previousrun.cpp", SOURCES)


if __name__ == "__main__":
    unittest.main()
