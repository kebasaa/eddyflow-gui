"""The run page's estimate of the time left holds for a split run, and covers
the whole run.

It was the main process's own periods times the periods left: a split pass
prints only its first piece, about one in every few of the range's periods,
so the estimate was the serial time while that piece ran, and then stood
still while the workers did the rest. Nor did it cover more than the pass
under way. The engine now tells the interface which passes lie ahead
("  Run plan: to pf rp (17520 periods)") and how far the pass under way has
got over every process ("  Progress [rp]: 1234 of 17520 periods done."), and
RunEta turns that into the time left until RP - and FCC, when it follows - are
done.

These are the engine's own lines, matched as text, so both sides are pinned.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_POOL = ENGINE_ROOT / "src" / "src_common" / "batch_pool.f90"
ENGINE_RP = ENGINE_ROOT / "src" / "src_rp" / "eddyflow-rp_main.f90"
ENGINE_FCC = ENGINE_ROOT / "src" / "src_fcc" / "eddyflow-fcc_main.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


PAGE = read(GUI_ROOT / "src" / "runpage.cpp")
HEADER = read(GUI_ROOT / "src" / "runpage.h")
ETA = read(GUI_ROOT / "src" / "runeta.cpp")
ETA_H = read(GUI_ROOT / "src" / "runeta.h")
SOURCES = read(GUI_ROOT / "sources.pri")

engine_available = ENGINE_POOL.is_file() and ENGINE_RP.is_file() and ENGINE_FCC.is_file()


def block(text, start):
    i = text.index(start)
    return text[i:text.index("return;", i)]


class ThePageReadsTheEngine(unittest.TestCase):

    def test_the_plan_and_the_progress_are_read_before_anything_else(self):
        # Ahead of every substring match below them, which could take one.
        plan_line = 'cleanLine.startsWith(QByteArrayLiteral("  Run plan:"))'
        progress_line = 'cleanLine.startsWith(QByteArrayLiteral("  Progress ["))'
        first_other = PAGE.index('QByteArrayLiteral("Performing planar-fit assessment")')
        self.assertLess(PAGE.index(plan_line), first_other)
        self.assertLess(PAGE.index(progress_line), first_other)
        self.assertIn("eta_.plan(stages, periods);", block(PAGE, plan_line))
        self.assertIn("eta_.progress(tag, done, total, eta_clock_.elapsed());",
                      block(PAGE, progress_line))

    def test_they_are_not_shown_in_the_console(self):
        i = PAGE.index("bool RunPage::filterData(")
        body = PAGE[i:PAGE.index("\n}\n", i)]
        self.assertIn('"  Progress ["', body)
        self.assertIn('"  Run plan:"', body)

    def test_the_old_per_step_estimate_is_gone(self):
        for gone in ("updateETC", "previous_elapsed_time", "main_progress_timer_",
                     ".replace(61, 12", "meanProcessingTimeMSec"):
            self.assertNotIn(gone, PAGE + HEADER, gone)

    def test_the_label_is_rebuilt_not_patched(self):
        i = PAGE.index("void RunPage::updateTimeEstimateLabels()")
        body = PAGE[i:PAGE.index("\n}\n", i)]
        self.assertIn("RunEta::formatDuration(elapsed)", body)
        self.assertIn("eta_.remainingMs(eta_clock_.elapsed())", body)
        self.assertIn("eta_.currentStageName()", body)
        # Ticks every second, so it counts down between progress lines.
        i = PAGE.index("void RunPage::updateElapsedTime()")
        self.assertIn("updateTimeEstimateLabels();", PAGE[i:PAGE.index("\n}\n", i)])

    def test_fcc_after_rp_continues_the_same_estimate(self):
        i = PAGE.index("void RunPage::startRun(")
        body = PAGE[i:PAGE.index("switch (runMode_)", i)]
        self.assertIn("fccFollows_ && rpSucceeded_ && eta_.running()", body)
        self.assertIn("ecProject_->isEngineStep2Needed()", body)
        self.assertIn("eta_.planFcc();", body)
        # RP says nothing at its end when FCC follows: the process's exit
        # tells, from MainWindow.
        i = PAGE.index("void RunPage::engineExited(bool success)")
        exited = PAGE[i:PAGE.index("\n}\n", i)]
        self.assertIn("rpSucceeded_ = success && fccFollows_;", exited)
        self.assertIn("eta_.engineFinished(eta_clock_.elapsed());", exited)
        main = read(GUI_ROOT / "src" / "mainwindow.cpp")
        i = main.index("void MainWindow::displayExitDialog()")
        self.assertLess(main.index("runPage()->engineExited(", i), main.index("runPage()->stopRun();", i))

    def test_the_pwb_rounds_share_the_bar(self):
        # The detection round would otherwise fill the bar before the
        # fluxes have begun.
        i = PAGE.index("void RunPage::setSplitProgress(")
        body = PAGE[i:PAGE.index("\n}\n", i)]
        self.assertIn('if (tag == QLatin1String("pd"))\n        fraction *= 0.4;', body)
        self.assertIn("fraction = 0.4 + 0.6 * fraction;", body)
        self.assertIn("if (value > progressValue_)", body)

    def test_a_pause_is_not_counted(self):
        self.assertIn("eta_.pause(eta_clock_.elapsed());", PAGE)
        self.assertIn("eta_.resume(eta_clock_.elapsed());", PAGE)

    def test_the_split_size_and_the_own_periods_feed_it(self):
        self.assertIn("eta_.setProcesses(", PAGE)
        self.assertIn("eta_.ownPeriod(eta_clock_.elapsed());",
                      block(PAGE, 'QByteArrayLiteral("processing new flux averaging period")'))

    def test_the_spectral_assessment_is_sized_by_the_engine_s_own_line(self):
        # "Starting Spectral Assessment" was never printed.
        self.assertNotIn('"Starting Spectral Assessment"', PAGE)
        self.assertIn('QByteArrayLiteral("binned (co)spectra from files")', PAGE)

    def test_the_time_lag_end_is_matched_as_printed(self):
        self.assertNotIn('"Time lag optimization session terminated"', PAGE)
        self.assertIn('QByteArrayLiteral("optimization session terminated")', PAGE)

    def test_the_estimator_is_built(self):
        self.assertIn("src/runeta.cpp", SOURCES)
        self.assertIn("src/runeta.h", SOURCES)
        self.assertIn("RunEta eta_;", HEADER)


class TheEstimator(unittest.TestCase):

    def test_it_knows_every_pass(self):
        for key in ("to", "pf", "rp", "sa", "fx"):
            self.assertIn('QStringLiteral("%s")' % key, ETA)
        # The detection round of a split production pass is part of it.
        self.assertIn('(tag == QLatin1String("pd")) ? QStringLiteral("rp") : tag', ETA)

    def test_it_times_the_pass_over_a_window(self):
        self.assertIn("constexpr qint64 WindowMs = 10 * 60 * 1000;", ETA)

    def test_hours_do_not_wrap_at_a_day(self):
        i = ETA.index("QString RunEta::formatDuration(")
        body = ETA[i:ETA.index("\n}\n", i)]
        self.assertNotIn("QTime", body)
        self.assertIn("s / 3600", body)

    def test_it_keeps_what_passes_took(self):
        self.assertIn("void store();", ETA_H)
        self.assertIn("settings->setValue(projectGroup()", ETA)
        self.assertIn("settings->setValue(ratioGroup()", ETA)


@unittest.skipUnless(engine_available, "engine sources not next to the GUI")
class TheEngineSaysWhatThePageReads(unittest.TestCase):

    def test_the_progress_line(self):
        pool = read(ENGINE_POOL)
        self.assertIn("call ConsoleSay('  Progress [' // trim(tag) // ']: ' // trim(DoneString) &", pool)
        self.assertIn("// ' of ' // trim(TotalString) // ' ' // trim(ProgUnit) // ' done.')", pool)

    def test_the_run_plans(self):
        self.assertIn("call ConsoleSay('  Run plan:' // trim(RunPlan) // ' (' // trim(TmpString1) // ' periods)')",
                      read(ENGINE_RP))
        fcc = read(ENGINE_FCC)
        self.assertIn("call ConsoleSay('  Run plan: sa fx')", fcc)
        self.assertIn("call ConsoleSay('  Run plan: fx')", fcc)

    def test_every_pass_reports(self):
        rp = read(ENGINE_RP)
        for stage in ("to", "pf", "rp"):
            self.assertRegex(rp, r"call ProgressBegin\('%s'," % stage)
        fcc = read(ENGINE_FCC)
        for stage in ("sa", "fx"):
            self.assertRegex(fcc, r"call ProgressBegin\('%s'," % stage)


if __name__ == "__main__":
    unittest.main()
