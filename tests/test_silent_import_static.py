"""Opening a pre-5.0.0 project upgrades it, saves it, and says nothing.

The migration itself already existed: loadEcProject notices a project with no
gas records, builds them from its flat columns, and folds the flat per-gas
settings onto them. It then stopped, marked the project modified, and told the
user to save - which left the upgrade half-done in two ways.

The project was unsaved, so the next run read the legacy shape again. And
resolveMigratedGasRecords - which fills in the species and analyser that the
project file alone cannot state, because they are only knowable from the
metadata - runs at the end of BasicSettingsPage::updateMetadataRead. Nothing
drove it. It happened if the user visited Basic Settings before saving, and
not otherwise, so a project could be saved with blank species.

What this file pins is the ORDER. `emit updateMetadataReadRequest()` is a
direct synchronous call - default AutoConnection, sender and receiver in one
thread - so the records are resolved before the save. Change that connection
to queued, or move the emit below the save, and the interface starts writing
projects with blank species with nothing to show for it. Source order is the
only thing cheap enough to assert here, so source order is asserted.

The backup is the other half. An upgrade the user did not ask for and cannot
see has to be one they can undo.
"""

from pathlib import Path
import re
import unittest


GUI_ROOT = Path(__file__).resolve().parents[1]

MAINWINDOW = GUI_ROOT / "src" / "mainwindow.cpp"
ECPROJECT = GUI_ROOT / "src" / "ecproject.cpp"
FILEUTILS = GUI_ROOT / "src" / "fileutils.cpp"


def _read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def _function(path, signature):
    """A function body, from its signature to the next column-0 closing brace."""
    src = _read(path)
    start = src.index(signature)
    end = src.index("\n}\n", start)
    return src[start:end]


class TheUpgradeIsDriven(unittest.TestCase):
    def test_open_no_longer_asks_the_user_to_finish_it(self):
        body = _function(MAINWINDOW, "bool MainWindow::openFile(")
        self.assertNotIn(
            "Save the project to complete the update.", body,
            "openFile still tells the user to finish the upgrade by hand")
        self.assertIn("upgradeProjectInPlace(filename)", body)

    def test_the_upgrade_is_keyed_on_the_loader_saying_so(self):
        """wasUpgradedOnLoad is the loader's own answer.

        It had no caller at all - the flag was set and never read - so the
        interface inferred the upgrade from the `modified` out-parameter,
        which is also set for an incompatible version.
        """
        body = _function(MAINWINDOW, "bool MainWindow::openFile(")
        self.assertIn("ecProject_->wasUpgradedOnLoad()", body)

    def test_the_records_are_resolved_before_anything_is_written(self):
        """The load-bearing assertion of this file."""
        body = _function(MAINWINDOW, "void MainWindow::upgradeProjectInPlace(")
        emit_at = body.index("emit updateMetadataReadRequest()")
        backup_at = body.index("FileUtils::backupFile(")
        save_at = body.index("fileSave(quiet)")
        self.assertLess(
            emit_at, backup_at,
            "the metadata must be read before the file is touched, or the "
            "records saved below still have blank species")
        self.assertLess(backup_at, save_at,
                        "the backup must be written before the save it exists "
                        "to undo")

    def test_the_resolve_is_a_direct_call(self):
        """A queued connection would satisfy the order above and break it.

        The emit only works because it runs synchronously. That is a property
        of the connection, not of openFile, so it is checked where it is made.
        """
        widget = _read(GUI_ROOT / "src" / "mainwidget.cpp")
        m = re.search(r"updateMetadataReadRequest[^;]*;", widget, re.S)
        self.assertIsNotNone(m, "updateMetadataReadRequest is no longer "
                                "connected in mainwidget.cpp")
        self.assertNotIn(
            "Qt::QueuedConnection", m.group(0),
            "a queued connection returns before the records are resolved, so "
            "the upgrade would save blank species")

    def test_a_failed_backup_stops_the_save(self):
        body = _function(MAINWINDOW, "void MainWindow::upgradeProjectInPlace(")
        guard = body[body.index("FileUtils::backupFile("):]
        self.assertIn("return;", guard[:guard.index("fileSave(quiet)")],
                      "an upgrade that cannot be undone must not be silent")

    def test_the_backup_keeps_the_original_in_place(self):
        body = _function(FILEUTILS, "bool FileUtils::backupFile(")
        self.assertIn("QFile::copy(fileName, backup)", body)
        self.assertNotIn("QFile::rename", body,
                         "the original must stay at its own path - it is what "
                         "Recent Files and the command line still name")


class TheLoaderFlagIsSane(unittest.TestCase):
    def test_it_is_cleared_on_every_load(self):
        """It was set once and never reset.

        Harmless while it only drove a dialog. Once it drives a backup and a
        save, a modern project opened after a legacy one in the same session
        would be rewritten for no reason.
        """
        body = _function(ECPROJECT, "bool EcProject::loadEcProject(")
        head = body[: body.index("bool isVersionCompatible")]
        self.assertIn("wasUpgradedOnLoad_ = false;", head,
                      "the flag must be cleared at entry, not left from the "
                      "previous project")

    def test_it_is_cleared_once_the_upgrade_has_landed(self):
        body = _function(MAINWINDOW, "void MainWindow::upgradeProjectInPlace(")
        self.assertIn("clearUpgradedOnLoad()", body)

    def test_the_modified_out_parameter_is_guarded(self):
        """It defaults to nullptr and was dereferenced unconditionally."""
        body = _function(ECPROJECT, "bool EcProject::loadEcProject(")
        for m in re.finditer(r"\*modified = true;", body):
            context = body[max(0, m.start() - 200):m.start()]
            self.assertIn(
                "modified != nullptr", context,
                "an unguarded *modified would crash any caller that omits it")


if __name__ == "__main__":
    unittest.main()
