"""Room on the output disk, checked by the interface before a run.

The engine watches the disk while it runs and stops below MinFreeOutputBytes,
because files written to a full network share come out empty with no error -
on CH-LAE, every file after the share filled, for a day. The interface asks
the same question before the run, and when the engine does stop for it, says
so instead of "an error has occurred".

Pinned: the two thresholds are one number; the interface recognises the
engine's own words; and both run paths that start from raw files ask first.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_POOL = GUI_ROOT.parent / "eddyflow-engine" / "src" / "src_common" / "batch_pool.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


HEADER = read(GUI_ROOT / "src" / "outputspace.h")
SPACE = read(GUI_ROOT / "src" / "outputspace.cpp")
MAIN = read(GUI_ROOT / "src" / "mainwindow.cpp")
SOURCES = read(GUI_ROOT / "CMakeLists.txt")


def body(src, signature):
    start = src.index(signature)
    end = src.index("\n}", start)
    return src[start:end]


class TheThresholdIsTheEngines(unittest.TestCase):

    def test_the_gui_floor_is_a_gibibyte(self):
        self.assertIn("constexpr qint64 MinFreeBytes = Q_INT64_C(1073741824);", HEADER)

    @unittest.skipUnless(ENGINE_POOL.is_file(), "engine repo not beside the GUI")
    def test_the_engine_floor_is_the_same(self):
        m = re.search(r"MinFreeOutputBytes = (\d+)_8", read(ENGINE_POOL))
        self.assertIsNotNone(m)
        self.assertEqual(m.group(1), "1073741824")


class TheEnginesWordsAreRecognised(unittest.TestCase):

    PHRASES = ("The disk this run writes to has only",
               "will not fit on the",
               "The output disk is full.",
               "Not enough room on the output disk")

    def test_the_gui_looks_for_each(self):
        fn = body(SPACE, "bool isDiskFullMessage(")
        for phrase in self.PHRASES:
            self.assertIn('"%s"' % phrase, fn)

    @unittest.skipUnless(ENGINE_POOL.is_file(), "engine repo not beside the GUI")
    def test_the_engine_still_says_each(self):
        pool = read(ENGINE_POOL)
        for phrase in self.PHRASES:
            self.assertIn(phrase, pool, "the engine no longer says %r" % phrase)

    def test_both_streams_are_watched(self):
        for fn in ("void MainWindow::updateConsoleReceived()",
                   "void MainWindow::updateConsoleError()"):
            self.assertIn("if (OutputSpace::isDiskFullMessage(newData)) { outputDiskFullSeen_ = true; }",
                          body(MAIN, fn))

    def test_both_exit_messages_say_so_and_the_flag_is_spent(self):
        for fn in ("void MainWindow::displayExitMsg(Process::ExitStatus exitReason)",
                   "void MainWindow::displayExitMsg2(Process::ExitStatus exitReason)"):
            self.assertIn("msgBox.setInformativeText(outputDiskFullInfo());",
                          MAIN[MAIN.index(fn):MAIN.index(fn) + 6000])
        self.assertIn("outputDiskFullSeen_ = false;",
                      body(MAIN, "void MainWindow::displayExitDialog()"))


class BothRawRunsAskFirst(unittest.TestCase):

    def test_express_asks(self):
        fn = MAIN[MAIN.index("void MainWindow::runExpress()"):]
        fn = fn[:fn.index("engineProcessStart(")]
        self.assertIn("if (!confirmOutputSpace(true)) { return; }", fn)

    def test_advanced_asks_and_knows_whether_it_starts_from_raw(self):
        fn = MAIN[MAIN.index("void MainWindow::runAdvancedStep_1()"):]
        fn = fn[:fn.index("engineProcessStart(")]
        self.assertIn("if (!confirmOutputSpace(ret == QMessageBox::Yes)) { return; }", fn)

    def test_a_full_disk_is_refused_and_raw_datasets_are_projected(self):
        fn = body(MAIN, "bool MainWindow::confirmOutputSpace(bool fromRaw)")
        self.assertIn("if (free < OutputSpace::MinFreeBytes)", fn)
        self.assertIn("OutputSpace::rawBytesPerPeriod(ecProject_,", fn)
        self.assertIn("if (need <= free - OutputSpace::MinFreeBytes) { return true; }", fn)

    def test_an_unanswerable_disk_lets_the_engine_decide(self):
        fn = body(MAIN, "bool MainWindow::confirmOutputSpace(bool fromRaw)")
        self.assertIn("if (free < 0) { return true; }", fn)

    def test_the_files_are_built(self):
        self.assertIn("src/outputspace.h", SOURCES)
        self.assertIn("src/outputspace.cpp", SOURCES)


if __name__ == "__main__":
    unittest.main()
