"""Four latent faults on the SmartFlux paths, found while tracing the crash.

None of them was the stack overflow. They are the things standing next to it:
a name built by searching the whole path for an extension, an index into a
button list taken from a project file, a button group with no parent, and a
failed load that left the mode switched on over the blank project it had just
put in place.
"""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def body(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start)]


class TheSmartfluxCopyIsNamedFromTheFileName(unittest.TestCase):
    """indexOf found the FIRST ".eddyflow" in the whole path, so a project
    inside a directory called something.eddyflow had the suffix spliced into
    the directory. A name with no extension gave -1, and insert() at -1 does
    nothing - so the copy came back identical to the original."""

    def setUp(self):
        self.win = read("src/mainwindow.cpp")

    def test_the_name_is_built_with_qfileinfo(self):
        fn = body(self.win, "QString MainWindow::smartfluxProjectNameFor")
        self.assertIn("QFileInfo info(filename)", fn)
        self.assertIn("completeBaseName()", fn)
        #> The splice that could land in a directory name is gone.
        self.assertNotIn("insert(", fn)
        self.assertNotIn("indexOf(", fn)

    def test_the_extension_is_always_the_project_one(self):
        fn = body(self.win, "QString MainWindow::smartfluxProjectNameFor")
        self.assertIn("Defs::PROJECT_FILE_EXT", fn)

    def test_the_old_splice_is_gone_from_the_loader(self):
        fn = body(self.win, "bool MainWindow::loadSmartfluxProjectCopy")
        self.assertIn("smartfluxProjectNameFor(filename)", fn)
        self.assertNotIn("insert(", fn)


class AFailedCopyLeavesSmartfluxMode(unittest.TestCase):
    """openFile calls fileClose() when a load fails, which swaps in a blank
    project. The mode used to stay on over it."""

    def setUp(self):
        self.win = read("src/mainwindow.cpp")

    def test_the_loader_reports_failure(self):
        self.assertIn("bool MainWindow::loadSmartfluxProjectCopy", self.win)
        self.assertIn("bool loadSmartfluxProjectCopy", read("src/mainwindow.h"))
        fn = body(self.win, "bool MainWindow::loadSmartfluxProjectCopy")
        #> fileOpen reports nothing, so the post-condition is what is checked.
        self.assertIn("currentProjectFile() == filenameCopy", fn)
        #> fileSaveAs already returned bool and the result was dropped.
        self.assertIn("if (!fileSaveAs(filenameCopy)) { return false; }", fn)

    def test_every_caller_acts_on_the_answer(self):
        """Three call sites, and all three could strand the mode."""
        calls = [l for l in self.win.splitlines()
                 if "loadSmartfluxProjectCopy(" in l
                 and "bool MainWindow::" not in l
                 and "smartfluxProjectNameFor" not in l]
        self.assertEqual(len(calls), 3, calls)
        for line in calls:
            self.assertIn("!loadSmartfluxProjectCopy(", line, line)

    def test_the_toggle_turns_the_mode_back_off(self):
        fn = body(self.win, "void MainWindow::setSmartfluxMode")
        #> Reassigns the parameter rather than returning, so the rest of the
        #> function restores every action and page the way leaving normally does.
        self.assertIn("on = false;", fn)
        self.assertIn("configState_.project.smartfluxMode = false;", fn)


class TheBiometRadioIsLookedUpById(unittest.TestCase):
    """generalUseBiomet comes straight from the project file. at(useBiom - 1)
    walked off a three-button list for any value above 3."""

    def setUp(self):
        self.page = read("src/projectpage.cpp")

    def test_no_positional_access_into_the_group(self):
        self.assertNotIn("biomRadioGroup->buttons().at(", self.page)

    def test_it_uses_button_and_checks_for_null(self):
        self.assertIn("biomRadioGroup->button(useBiom - 1)", self.page)
        self.assertIn("biomButton != nullptr", self.page)
        self.assertIn("if (biomButton)", self.page)


class EveryButtonGroupHasAParent(unittest.TestCase):
    """A QButtonGroup with no parent is never deleted with the page that owns
    it. Its two siblings on the same page always passed one."""

    def test_no_parentless_group_on_the_project_page(self):
        page = read("src/projectpage.cpp")
        self.assertNotIn("new QButtonGroup;", page)
        self.assertIn("fileTypeRadioGroup = new QButtonGroup(this);", page)


if __name__ == "__main__":
    unittest.main()
