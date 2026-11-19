"""An Advanced run checks the assessment files it will read before it starts.

A project names its planar fit, time lag and spectral assessment files by path.
When one had been moved or renamed nothing noticed: the engine started, could
not open it, and stopped. Now getRunAdvanced asks resolveMissingInputFiles
first. When a file the run will use is gone, InputFilesDialog shows one line
per file - the same Browse and Remote drive picker the settings pages use, the
line pre-filled with a similar file found nearby, lines that do not apply
greyed out, and a line whose path is not the project's saved one marked - and
the project is saved with whatever changed.

Pinned: where the check runs; that it uses testBeforeRunningPassed's own
conditions for each file; that remote and empty paths are left alone; the
engine's file-name timestamp and paddings; that each line is a remote-capable
FileBrowseWidget; that a changed path is highlighted; that Continue waits for
every applicable file; and that nothing is written to the project until the
window is accepted, and then only what changed.
"""

import unittest
from pathlib import Path

GUI = Path(__file__).resolve().parents[1]
SRC = GUI / "src"


def read(name):
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


MAIN = read("mainwindow.cpp")
CHECK = read("inputfilecheck.cpp")
CHECK_H = read("inputfilecheck.h")
DIALOG = read("inputfilesdialog.cpp")
CMAKE = (GUI / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")


def body(src, signature):
    a = src.index(signature)
    return src[a:src.index("\n}\n", a)]


RESOLVE = body(MAIN, "bool MainWindow::resolveMissingInputFiles()")


class WhereTheCheckRuns(unittest.TestCase):

    def test_before_the_date_range_and_the_run(self):
        fn = body(MAIN, "void MainWindow::getRunAdvanced()")
        call = fn.index("!resolveMissingInputFiles()")
        self.assertLess(call, fn.index("getDatesRangeDialog(Defs::CurrRunMode::Advanced)"))
        self.assertIn("return;", fn[call:call + 80])

    def test_the_files_are_built(self):
        for f in ("inputfilecheck.h", "inputfilecheck.cpp",
                  "inputfilesdialog.h", "inputfilesdialog.cpp"):
            self.assertIn("src/" + f, CMAKE)


class WhenEachFileApplies(unittest.TestCase):

    def test_the_same_conditions_as_the_load_a_file_messages(self):
        self.assertIn("ecProject_->screenRotMethod() >= 3", RESOLVE)
        self.assertIn("ecProject_->planarFitMode() == 0", RESOLVE)
        self.assertIn("(tlagMeth == 4 || tlagMeth == 5)", RESOLVE)
        self.assertIn("ecProject_->timelagOptMode() == 0", RESOLVE)
        self.assertIn("ecProject_->generalHfMethod() > 1", RESOLVE)
        self.assertIn("ecProject_->generalHfMethod() < 5", RESOLVE)
        self.assertIn("ecProject_->spectraMode() == 0", RESOLVE)

    def test_empty_and_remote_paths_are_left_alone(self):
        self.assertIn("line.saved.trimmed().isEmpty()", RESOLVE)
        self.assertIn("RemoteSource::isRemote(line.saved)", RESOLVE)
        self.assertIn("if (!anyMissing) { return true; }", RESOLVE)


class ASimilarFile(unittest.TestCase):

    def test_the_engines_timestamp_becomes_a_wildcard(self):
        self.assertIn('"\\\\d{4}-\\\\d{2}-\\\\d{2}T\\\\d{6}"', CHECK)

    def test_the_engines_paddings(self):
        for padding in ("_planar_fit", "_timelag_opt", "_pwb_timelag",
                        "_spectral_assessment"):
            self.assertIn('"%s"' % padding, CHECK_H)

    def test_a_pwb_table_is_never_offered_its_summary(self):
        self.assertIn('QStringLiteral("_pwb_timelag_opt_")', RESOLVE)

    def test_newest_first_one_level_only(self):
        fn = body(CHECK, "QString findSimilar(")
        self.assertIn("file.lastModified() > best.lastModified()", fn)
        dirs = body(CHECK, "QStringList searchDirs(")
        self.assertIn("QDir::Dirs | QDir::NoDotAndDotDot", dirs)
        self.assertNotIn("Subdirectories", dirs)


class TheWindow(unittest.TestCase):

    def test_each_line_is_the_settings_pages_picker(self):
        self.assertIn("auto browse = new FileBrowseWidget;", DIALOG)
        self.assertIn("browse->setRemoteBrowseEnabled(true);", DIALOG)
        for key in ("planar_fit_file", "timelag_file", "spectral_assessment_file"):
            self.assertIn('QStringLiteral("%s")' % key, DIALOG)

    def test_lines_that_do_not_apply_are_greyed(self):
        self.assertIn("browse->setEnabled(lines_[k].applies);", DIALOG)
        self.assertIn("Not used with the current settings.", DIALOG)

    def test_a_changed_path_is_highlighted(self):
        fn = body(DIALOG, "void InputFilesDialog::updateLine(Kind kind)")
        self.assertIn("isChanged ? QString::fromLatin1(ChangedFieldStyle)", fn)
        self.assertIn("Changed from the project's saved path.", fn)

    def test_continue_waits_for_every_applicable_file(self):
        fn = body(DIALOG, "void InputFilesDialog::updateContinue()")
        self.assertIn("if (!lines_[k].applies) { continue; }", fn)
        self.assertIn("InputFileCheck::usable(path(static_cast<Kind>(k)))", fn)


class NothingIsWrittenUntilAccepted(unittest.TestCase):

    def test_setters_and_save_follow_the_dialog(self):
        accepted = RESOLVE.index("if (dialog.exec() != QDialog::Accepted) { return false; }")
        for setter in ("setPlanarFitFile(", "setTimelagOptFile(", "setSpectraFile("):
            self.assertGreater(RESOLVE.index(setter), accepted, setter)
        self.assertGreater(RESOLVE.index("return fileSaveSilently();"), accepted)

    def test_only_what_changed(self):
        self.assertIn("if (dialog.changed(Kind::PlanarFit))", RESOLVE)
        self.assertIn("if (dialog.changed(Kind::Timelag))", RESOLVE)
        self.assertIn("if (dialog.changed(Kind::SpectralAssessment))", RESOLVE)
        self.assertIn("if (!anyChanged) { return true; }", RESOLVE)
        self.assertNotIn("setPlanarFitFile", DIALOG)


if __name__ == "__main__":
    unittest.main()
