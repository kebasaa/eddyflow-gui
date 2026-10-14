"""The time lag preliminary assessment, on the Advanced Settings page.

A button beside the time lag settings opens a window that runs the engine on a
random sample of the half-hours, over a wide search window that ignores the
nominal, minimum and maximum lags of the metadata. It charts the lags found for
each gas, suggests a nominal lag and a window, and writes them, edited or not,
to the metadata.

What this pins down:
- it needs a raw data folder, and says so rather than opening;
- the run is on a throw-away copy of the project, in a folder of its own with
  a home folder of its own, so the user's files - and whether they count as
  saved - are never touched, and a main-window run never meets it;
- the copy runs advanced, over the whole dataset, without FCC, and carries the
  engine's sampling keys, which nothing writes into a user's project;
- its outputs go unless the user asks to keep them;
- progress is read the way the run page reads it.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"


def read(path):
    return Path(path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def function(path, signature):
    text = read(path)
    i = text.index(signature)
    return text[i:text.index("\n}\n", i)]


ADV = GUI_ROOT / "src" / "advprocessingoptions.cpp"
DIALOG = GUI_ROOT / "src" / "timelagpreassessdialog.cpp"
STATS = GUI_ROOT / "src" / "lagstats.cpp"


class TheButtonSitsBesideTheTimeLagSettings(unittest.TestCase):

    def test_it_is_on_the_time_lag_row_in_a_column_of_its_own(self):
        self.assertIn("settingsLayout->addWidget(tlPreassessButton, 13, 4);", read(ADV))
        self.assertIn('tr("Timelag preliminary assessment...")', read(ADV))

    def test_it_follows_the_time_lag_checkbox(self):
        self.assertIn("tlPreassessButton, &QPushButton::setEnabled);", read(ADV))
        self.assertIn("tlPreassessButton->setEnabled(ecProject_->screenTlagMeth() != 0);", read(ADV))

    def test_without_raw_data_it_says_so_instead_of_opening(self):
        body = function(ADV, "void AdvProcessingOptions::showTlPreassessDialog()")
        gate = body.index("TimelagPreassessDialog::rawDataProblem(ecProject_)")
        self.assertLess(gate, body.index("new TimelagPreassessDialog("))
        self.assertIn('tr("Raw Data Folder Not Set")', body)
        self.assertLess(body.index("WidgetUtils::warning("), body.index("return;"))

    def test_the_run_checks_again(self):
        body = function(DIALOG, "void TimelagPreassessDialog::runOrCancel()")
        self.assertIn("rawDataProblem(ecProject_)", body)

    def test_an_empty_missing_or_fileless_folder_is_a_problem(self):
        body = function(DIALOG, "QString TimelagPreassessDialog::rawDataProblem(")
        self.assertIn("path.isEmpty()", body)
        self.assertIn("RemoteSource::isRemote(path)", body)
        self.assertIn("isDir()", body)
        self.assertIn("it.hasNext()", body)


class TheUsersFilesAreNotTouched(unittest.TestCase):

    def test_the_copies_keep_the_saved_flag_as_it_was(self):
        body = function(DIALOG, "bool TimelagPreassessDialog::writeRunProject(")
        self.assertIn("dlProject_->saveCopy(metadataPath)", body)
        self.assertIn("ecProject_->saveEcProjectCopy(*projectPath)", body)
        self.assertNotIn("saveProject(", body)
        self.assertNotIn("saveEcProject(", body)
        for path, name in ((GUI_ROOT / "src" / "dlproject.cpp", "bool DlProject::saveCopy("),
                           (GUI_ROOT / "src" / "ecproject.cpp", "bool EcProject::saveEcProjectCopy(")):
            copy = function(path, name)
            self.assertIn("modified_ = wasModified;", copy)
            self.assertNotIn("setModified(", copy, "restoring through setModified sets off the autosave")

    def test_the_copy_names_the_copied_metadata(self):
        body = function(DIALOG, "bool TimelagPreassessDialog::writeRunProject(")
        self.assertIn("ini.setValue(EcIni::INI_PROJECT_10, metadataPath)", body)

    def test_the_engine_has_a_folder_and_a_home_of_its_own(self):
        run = function(DIALOG, "void TimelagPreassessDialog::runOrCancel()")
        self.assertIn("std::make_unique<QTemporaryDir>(", run)
        start = function(DIALOG, "void TimelagPreassessDialog::startEngine(")
        self.assertIn('workDir_->path() + QStringLiteral("/home")', start)
        self.assertNotIn("appEnvPath", start)


class TheCopyRunsWhatTheAssessmentNeeds(unittest.TestCase):

    def setUp(self):
        self.body = function(DIALOG, "bool TimelagPreassessDialog::writeRunProject(")

    def test_advanced_whole_dataset_no_fcc(self):
        # Express mode would replace the time lag method with its own.
        self.assertIn("ini.setValue(EcIni::INI_PROJECT_33, 0);", self.body)
        self.assertIn("ini.setValue(EcIni::INI_PROJECT_40, 0);", self.body)
        self.assertIn("ini.setValue(EcIni::INI_PROJECT_54, 0);", self.body)
        self.assertIn("ini.setValue(EcIni::INI_PROJECT_60, 0);", self.body)
        self.assertIn("ini.setValue(EcIni::INI_PROJECT_65, 0);", self.body)

    def test_an_assessment_only_pre_pass_over_the_search_window(self):
        self.assertIn("ini.setValue(EcIni::INI_SCREEN_SETTINGS_7, pwb ? TlagPwb : TlagOptimizer);", self.body)
        self.assertIn("ini.setValue(EcIni::INI_TIMELAG_OPT_2, 1);", self.body)
        self.assertIn("ini.setValue(EcIni::INI_TIMELAG_OPT_21, 1);", self.body)
        self.assertIn('QStringLiteral("gas_%1_to_")', self.body)
        self.assertIn('QStringLiteral("gas_%1_pwb_")', self.body)

    def test_the_sampling_keys_are_the_engines(self):
        keys = re.findall(r'ini\.setValue\(QStringLiteral\("(to_sample_\w+|to_dump_lags)"\)', self.body)
        self.assertEqual(sorted(keys), ["to_dump_lags", "to_sample_frac", "to_sample_min_n", "to_sample_seed"])
        globals_ = ENGINE_ROOT / "src" / "src_rp" / "m_rp_global_var.f90"
        if not globals_.exists():
            self.skipTest("engine checkout not beside the GUI")
        for key in keys:
            self.assertRegex(read(globals_), r"SNTags\(\d+\)%%Label\s*/\s*'%s'" % key)

    def test_nothing_else_writes_them(self):
        for f in (GUI_ROOT / "src").rglob("*.cpp"):
            if f.name == "timelagpreassessdialog.cpp":
                continue
            self.assertNotIn("to_sample_", read(f), f.name)


class TheOutputsGoUnlessKept(unittest.TestCase):

    def test_kept_only_when_asked(self):
        self.assertIn('tr("Keep pre-assessment outputs")', read(DIALOG))
        finished = function(DIALOG, "void TimelagPreassessDialog::engineFinished()")
        self.assertIn("if (keepCheck_->isChecked()) { keepOutputs(); }", finished)
        close = function(DIALOG, "void TimelagPreassessDialog::reject()")
        self.assertLess(close.index("keepOutputs()"), close.index("discardOutputs()"))

    def test_kept_beside_the_project_outputs(self):
        keep = function(DIALOG, "void TimelagPreassessDialog::keepOutputs()")
        self.assertIn("outputBaseDir()", keep)
        self.assertIn('QStringLiteral("/tlag_preassessment_")', keep)
        self.assertIn("ecProject_->generalOutPath()",
                      function(DIALOG, "QString TimelagPreassessDialog::outputBaseDir()"))

    def test_keeping_saves_each_chart_and_the_suggestions(self):
        keep = function(DIALOG, "void TimelagPreassessDialog::keepOutputs()")
        self.assertIn("saveFigures(keptDir_);", keep)
        figures = function(DIALOG, "void TimelagPreassessDialog::saveFigures(")
        self.assertIn('QStringLiteral("_timelag_histogram.png")', figures)
        self.assertIn("figure.renderImage(FigureSize)", figures)
        self.assertIn('QStringLiteral("/suggested_timelags.csv")', figures)
        self.assertIn(",note", figures)
        # In light colours whatever the theme.
        render = function(GUI_ROOT / "src" / "laghistogramwidget.cpp",
                          "QImage LagHistogramWidget::renderImage(")
        self.assertIn("light.setColor(QPalette::Base, Qt::white);", render)
        self.assertIn("setPalette(themed);", render)


class TheSampleIsALogSlider(unittest.TestCase):

    SLIDER = GUI_ROOT / "src" / "logpercentslider.cpp"

    def test_a_decade_per_third_from_a_tenth_to_all(self):
        self.assertIn("setRange(0, 3 * StepsPerDecade);", read(self.SLIDER))
        self.assertIn("std::pow(10.0, static_cast<double>(position) / StepsPerDecade - 1.0)",
                      function(self.SLIDER, "double LogPercentSlider::toPercent("))

    def test_the_default_is_marked_and_double_click_returns_to_it(self):
        paint = function(self.SLIDER, "void LogPercentSlider::paintEvent(")
        self.assertIn("toPosition(default_)", paint)
        self.assertIn("setPercent(default_);",
                      function(self.SLIDER, "void LogPercentSlider::mouseDoubleClickEvent("))
        self.assertIn("slider_->setDefaultPercent(pct);",
                      function(DIALOG, "void TimelagPreassessDialog::refresh()"))


class TheEntireDataset(unittest.TestCase):

    def test_the_box_pins_the_slider_and_gives_its_value_back(self):
        body = function(DIALOG, "void TimelagPreassessDialog::wholeToggled(")
        self.assertIn("percentBeforeWhole_ = slider_->percent();", body)
        self.assertIn("slider_->setPercent(LogPercentSlider::MaxPercent);", body)
        self.assertIn("slider_->setPercent(percentBeforeWhole_);", body)

    def test_no_sample_keys_and_the_projects_humidity_classes(self):
        body = function(DIALOG, "bool TimelagPreassessDialog::writeRunProject(")
        guarded = body[body.index("if (!runWhole_)"):body.index("if (!pwb)")]
        for key in ("to_sample_frac", "to_sample_min_n", "to_sample_seed", "to_h2o_nclass"):
            self.assertIn(key, guarded)
        self.assertNotIn("to_dump_lags", guarded)

    def test_its_optimisation_file_is_left_in_the_output_folder_kept_or_not(self):
        finished = function(DIALOG, "void TimelagPreassessDialog::engineFinished()")
        copy_at = finished.index("if (success && runWhole_) { copyOptimisationFiles(); }")
        self.assertLess(copy_at, finished.index("if (keepCheck_->isChecked())"))
        copy = function(DIALOG, "void TimelagPreassessDialog::copyOptimisationFiles()")
        for pattern in ('"*_pwb_timelag_*.csv"', '"*_pwb_timelag_opt_*.txt"', '"*_timelag_opt_*.txt"'):
            self.assertIn(pattern, copy)
        self.assertIn("outputBaseDir()", copy)

    def test_use_in_project_points_the_project_at_it(self):
        body = function(DIALOG, "void TimelagPreassessDialog::useFileInProject()")
        self.assertIn("ecProject_->setTimelagOptMode(0);", body)
        self.assertIn("ecProject_->setTimelagOptFile(optFileForProject_);", body)
        # The PWB table only fits the windows it was detected with.
        self.assertIn("gases[i].proc.pwbMinLag = it.value().first;", body)

    def test_new_pwb_windows_would_strand_that_table(self):
        body = function(DIALOG, "void TimelagPreassessDialog::setInMetadata()")
        self.assertIn("ecProject_->timelagOptFile()", body)
        self.assertIn("WidgetUtils::yesNoQuestion(", body)
        self.assertIn("if (leftTable) { ecProject_->setTimelagOptMode(1); }", body)


class TheGasesAreColumns(unittest.TestCase):

    def test_two_tables_with_the_settings_as_rows(self):
        body = read(DIALOG)
        self.assertIn('createGasTable({ tr("From [s]"), tr("To [s]") }, 1, this)', body)
        self.assertIn('createGasTable({ tr("Lags used"), tr("Nominal [s]"), tr("Minimum [s]"), tr("Maximum [s]") },',
                      body)
        build = function(DIALOG, "void TimelagPreassessDialog::buildGasRows()")
        self.assertIn("windowTable_->setColumnCount(rows_.size());", build)
        self.assertIn("resultTable_->setColumnCount(rows_.size());", build)

    def test_no_vertical_scrolling(self):
        self.assertIn("table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);", read(DIALOG))
        self.assertIn("table->verticalHeader()->length()",
                      function(DIALOG, "void TimelagPreassessDialog::fitTableHeight("))

    def test_remarks_are_tooltips_on_the_column(self):
        note = function(DIALOG, "void TimelagPreassessDialog::applyNote(")
        self.assertIn("header->setToolTip(row.note);", note)
        self.assertIn("item->setToolTip(row.note);", note)
        self.assertIn("QStyle::SP_MessageBoxWarning", note)
        self.assertNotIn("row.note->setText", read(DIALOG))


class AlwaysParallel(unittest.TestCase):

    def test_split_across_the_cores_whatever_the_main_window_says(self):
        start = function(DIALOG, "void TimelagPreassessDialog::startEngine(")
        self.assertIn('QStringLiteral("-j"), QStringLiteral("0"),', start)
        self.assertNotIn("parallelPrepass", read(DIALOG))


class TheProgressIsShown(unittest.TestCase):

    def test_busy_until_the_first_progress_line_then_counted(self):
        start = function(DIALOG, "void TimelagPreassessDialog::startEngine(")
        self.assertIn("progressBar_->setRange(0, 0);", start)
        read_ = function(DIALOG, "void TimelagPreassessDialog::readEngineOutput()")
        self.assertIn("EngineProgress::parse(line, &tag, &done, &total)", read_)
        self.assertIn("progressBar_->setRange(0, static_cast<int>(total));", read_)


class TheSuggestionsAndWhereTheyGo(unittest.TestCase):

    def test_the_optimisers_rule(self):
        body = function(STATS, "Suggestion suggest(")
        self.assertIn("pgRange * std::max(mad(lags), 0.1) / 0.6745", body)
        self.assertIn("if (closedPath) { s.minimum = std::max(0.0, s.minimum); }", body)

    def test_the_default_share(self):
        body = function(STATS, "double defaultSamplePercent(")
        self.assertIn("if (periods <= month) { return 10.0; }", body)
        self.assertIn("if (periods >= year) { return 1.0; }", body)

    def test_written_to_the_gas_column_and_the_methods_own_window(self):
        body = function(DIALOG, "void TimelagPreassessDialog::setInMetadata()")
        self.assertIn("gases.at(i).rawColumn - 1", body)
        for setter in ("setNomTimelag(nom)", "setMinTimelag(lo)", "setMaxTimelag(hi)"):
            self.assertIn(setter, body)
        self.assertIn("gases[i].proc.toMinLag = floorTenth(lo);", body)
        self.assertIn("gases[i].proc.pwbMaxLag = ceilTenth(hi);", body)
        self.assertIn("dlProject_->setModified(true);", body)
        self.assertIn("ecProject_->setGasColumns(gases);", body)


if __name__ == "__main__":
    unittest.main()
