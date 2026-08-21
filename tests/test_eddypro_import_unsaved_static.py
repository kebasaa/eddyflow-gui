"""Importing an EddyPro project writes nothing until the user saves.

Opening a `.eddypro` used to write two files before the user had seen the
project. `<base>.eddyflow`, deliberately - and then the user's own `.metadata`,
by accident: reopening drove `DlProject::loadProject` over it, any retired
Campbell model key set `legacyInstrumentSlugs`, that called `setModified(true)`,
and `projectModified` is wired to `DlIniDialog::saveAvailable`, which writes.
Opening a project to look at it is not consent to rewrite it, and there was no
backup of the metadata to undo it with.

Now the conversion happens in memory and stops, leaving the same state File >
New leaves: modified, `newFlag_` true, never saved. Everything downstream of
that already works - `fileSave` routes to `fileSaveAs`, `continueBeforeClose`
prompts, the Run paths refuse until the project is on disk - so what is worth
pinning is not the machinery but the three decisions that keep it honest:

  * nothing is written by the import itself;
  * the metadata is HELD rather than skipped, and written on the first project
    save to a name that cannot be the file it was read from;
  * the records are resolved from the metadata before any of it is shown,
    because a `.eddypro` states a raw column number and nothing else - the
    fourth gas slot's species is knowable only from the metadata, and a record
    saved without it is meaningless to the engine.

`test_silent_import_static.py` covers the neighbouring case that did NOT
change: a legacy pre-5.0.0 `.eddyflow` is still backed up and upgraded in
place, because there the file is one the user already chose and the `.bak`
makes the rewrite undoable.
"""

import re
import unittest
from pathlib import Path


GUI_ROOT = Path(__file__).resolve().parents[1]

MAINWINDOW = GUI_ROOT / "src" / "mainwindow.cpp"
DLINIDIALOG = GUI_ROOT / "src" / "dlinidialog.cpp"
DLPROJECT = GUI_ROOT / "src" / "dlproject.cpp"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def function(path, signature):
    """A function body, from its signature to the next column-0 closing brace."""
    src = read(path)
    start = src.index(signature)
    end = src.index("\n}\n", start)
    return src[start:end]


class TheImportWritesNothing(unittest.TestCase):

    def setUp(self):
        self.body = function(MAINWINDOW,
                             "void MainWindow::importEddyProFile(")

    def test_it_does_not_save_the_project(self):
        self.assertNotIn("saveEcProject", self.body,
                         "the import writes the project again")

    def test_it_does_not_reopen_anything(self):
        #> Reopening is what drove the metadata rewrite, and it is also what
        #> leaked openingFlag_: openFile sets it and leaves clearing to its
        #> caller, which this function never did, so every later open in the
        #> session short-circuited.
        self.assertNotIn("openFile(", self.body,
                         "the import still hands off to the open path")

    def test_it_leaves_an_unsaved_project(self):
        #> newFlag_ is the interface's own idea of "never saved" - the same one
        #> File > New sets - and every consumer already keys on it.
        self.assertIn("newFlag_ = true", self.body)
        self.assertIn("setCurrentProjectFile(", self.body)

    def test_the_project_is_marked_modified(self):
        #> loadEcProject ends with setModified(false), so an imported project
        #> is clean unless something re-dirties it. setCurrentProjectFile's
        #> modified arm does exactly that - it sets generalFileName without
        #> blockSignals - and without it the close prompt never fires and the
        #> work is lost in silence.
        m = re.search(r"setCurrentProjectFile\(\s*targetFile\s*,\s*(\w+)\s*\)",
                      self.body)
        self.assertIsNotNone(m, "the project file is not set with a modified flag")
        flag = m.group(1)
        self.assertRegex(self.body, r"%s\s*=\s*true" % re.escape(flag),
                         "the modified flag passed is not true")


class TheRecordsAreResolvedFirst(unittest.TestCase):
    """The same ordering argument test_silent_import_static.py makes."""

    def setUp(self):
        self.body = function(MAINWINDOW,
                             "void MainWindow::importEddyProFile(")

    def test_the_metadata_is_read_before_the_project_is_shown(self):
        #> updateMetadataReadRequest is a direct synchronous call, and it is
        #> the only thing that fills in each gas record's species and analyser.
        #> Shown before it runs, the user is looking at blank species - and
        #> would save them.
        emit_at = self.body.index("emit updateMetadataReadRequest()")
        show_at = self.body.index("setCurrentProjectFile(")
        self.assertLess(emit_at, show_at,
                        "the project is shown before its records are resolved")

    def test_the_metadata_is_held_before_it_is_read(self):
        #> Reading it is what marks it modified, and modified is what writes
        #> it. A beginDeferredSave after the read is too late by one signal.
        hold_at = self.body.index("beginDeferredSave()")
        emit_at = self.body.index("emit updateMetadataReadRequest()")
        self.assertLess(hold_at, emit_at,
                        "the metadata is read before it is held, so the read "
                        "can still write it")

    def test_a_metadata_that_is_not_there_falls_back_to_the_sibling(self):
        #> proj_file is an absolute path from the machine the project was made
        #> on. Without this an imported project that has been copied anywhere
        #> has no metadata at all, and therefore blank species - the engine's
        #> own importer has the same rule.
        self.assertIn("generalMdFilepath()", self.body)
        self.assertIn("setGeneralMdFilepath(", self.body)
        self.assertIn("METADATA_FILE_EXT", self.body)


class TheMetadataIsHeldNotSkipped(unittest.TestCase):

    def test_the_autosave_is_suppressed_while_deferred(self):
        body = function(DLINIDIALOG, "void DlIniDialog::saveAvailable()")
        guard = body[: body.index("if (newFlag_)")]
        self.assertIn("deferredSave_", guard)
        self.assertIn("return;", guard,
                      "saveAvailable still reaches apply() while deferred")

    def test_writing_still_has_exactly_one_route(self):
        #> DlProject::saveProject is called from DlIniDialog::saveFile and
        #> nowhere else. That is what makes suppressing one slot sufficient; a
        #> second caller would be a second way to write the user's file.
        callers = [f for f in (GUI_ROOT / "src").rglob("*.cpp")
                   if "saveProject(" in read(f) and f.name != "dlproject.cpp"]
        self.assertEqual(["dlinidialog.cpp"], sorted(f.name for f in callers))

    def test_the_commit_resumes_normal_saving(self):
        #> After the first write it is an ordinary open metadata file, or every
        #> later edit would be held too and silently lost on close.
        body = function(DLINIDIALOG, "bool DlIniDialog::commitDeferredSave(")
        self.assertIn("deferredSave_ = false", body)
        self.assertIn("newFlag_ = false", body)
        self.assertIn("filename_ = path", body)


class TheProjectSaveWritesBoth(unittest.TestCase):

    def setUp(self):
        self.body = function(MAINWINDOW, "bool MainWindow::saveFile(")

    def test_the_metadata_is_written_before_the_project(self):
        #> proj_file has to name the metadata in the project being written, so
        #> the commit and setGeneralMdFilepath both precede saveEcProject.
        commit_at = self.body.index("commitDeferredSave(")
        point_at = self.body.index("setGeneralMdFilepath(")
        save_at = self.body.index("saveEcProject(")
        self.assertLess(commit_at, point_at)
        self.assertLess(point_at, save_at,
                        "the project is written before proj_file names the "
                        "metadata it was given")

    def test_a_failed_metadata_write_stops_the_save(self):
        guard = self.body[self.body.index("commitDeferredSave("):]
        self.assertIn("return false;", guard[:guard.index("saveEcProject(")],
                      "the project is saved even though its metadata was not")

    def test_the_target_cannot_be_the_file_it_was_read_from(self):
        #> On a straight import the project is offered the .eddypro's own base
        #> name, so <base>.metadata IS the metadata that was read. Writing
        #> there would destroy the file the user opened.
        body = function(MAINWINDOW, "QString MainWindow::importedMetadataTarget(")
        self.assertIn("canonicalFilePath()", body,
                      "the collision test compares strings, and the two "
                      "spellings reach it from different places")
        self.assertIn("_ep_imported", body)


class TheImporterIsReachable(unittest.TestCase):

    def test_every_route_redirects_a_eddypro(self):
        #> The redirect used to sit inside the `fileName.isEmpty()` branch, so
        #> only the file dialog reached it. A .eddypro from the command line,
        #> from Recent Files or from the macOS Finder fell through to openFile
        #> and was rejected as "not in EddyFlow native format".
        body = function(MAINWINDOW, "void MainWindow::fileOpen(")
        redirect_at = body.index("importEddyProFile(fileStr)")
        else_at = body.index("// programmatically")
        self.assertGreater(redirect_at, else_at,
                           "the redirect is still inside the interactive-only "
                           "branch")

    def test_the_finder_accepts_our_own_extension(self):
        #> It accepted .eddypro and dropped .eddyflow, so the fork's own
        #> projects could not be double-clicked on macOS.
        src = read(GUI_ROOT / "src" / "main.cpp")
        m = re.search(r"requestedFile\.endsWith\([^)]*\)", src)
        self.assertIsNotNone(m, "the Finder filter is gone")
        window = src[max(0, m.start() - 400): m.start() + 400]
        self.assertIn("PROJECT_FILE_EXT", window,
                      "the Finder filter still names only .eddypro")


class TheUnsavedStateIsNotWrittenBehindTheUsersBack(unittest.TestCase):

    def test_the_silent_metadata_cleanup_skips_an_unsaved_project(self):
        #> It clears proj_file and calls fileSave(quiet), which for a project
        #> that has never been saved is fileSaveAs - a save dialog in front of
        #> somebody who has only just opened something.
        body = function(MAINWINDOW, "void MainWindow::silentMdClenaup()")
        head = body[: body.index("scheduledSilentMdCleanup_")]
        self.assertIn("newFlag_", head)
        self.assertIn("return;", head)


if __name__ == "__main__":
    unittest.main()
