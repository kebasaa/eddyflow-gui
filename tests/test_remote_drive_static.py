"""Inputs may come from a shared Google Drive or Dropbox link; outputs may not.

A field that takes an input gets a "Remote drive..." button beside "Browse...".
It asks for the link to the drive the first time, then opens a browser over
that drive; the same drive serves every other field until "Different drive...".
The engine downloads what it needs itself (remote_source.f90 in
eddyflow-engine); the interface only browses, lists and fetches the odd file it
reads for itself.

What is held here, because each has a way to fail silently:

**A link survives the trip into the project.** `QDir::cleanPath` turns
`https://` into `https:/`, and `QFileInfo::canonicalFilePath` of a link is
empty, so the old `setPath` blanked it. Every slot that stores an input goes
through `RemoteSource::cleanPath`, and `setPath` keeps a link whole.

**What the interface opens is downloaded first.** Listing a shared folder gives
cache paths with nothing behind them yet. The GHG embedded-metadata probe, the
biomet readers and the ancillary file tests read files, so each asks
`RemoteSource::ensureLocal` for a local one.

**The metadata file is copied next to the project.** EddyFlow updates it, and a
link is read only.

**Outputs stay local.** The output folder never gets the button, and a widget
without it refuses a link with a warning window.

**The tooltip is the one the user asked for.** "Dropbox or Google Drive link".
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"

INPUT_WIDGETS = {
    "basicsettingspage.cpp": ["datapathBrowse"],
    "projectpage.cpp": ["metadataFileBrowse", "dynamicMdFileBrowse",
                        "biometExtFileBrowse", "biometExtDirBrowse"],
    "advspectraloptions.cpp": ["binnedSpectraDirBrowse", "spectraFileBrowse",
                               "fullSpectraDirBrowse"],
    "planarfitsettingsdialog.cpp": ["fileBrowse"],
    "timelagsettingsdialog.cpp": ["fileBrowse"],
    "pwbtimelagsettingsdialog.cpp": ["fileBrowse"],
    "advprocessingoptions.cpp": ["headCorrDirBrowse"],
}

OUTPUT_WIDGETS = {
    "basicsettingspage.cpp": ["outpathBrowse"],
    "createpackagedialog.cpp": ["outpathBrowse"],
}

#: slot file -> the setters that store an input path
STORE_SITES = {
    "basicsettingspage.cpp": ["setScreenDataPath"],
    "projectpage.cpp": ["setGeneralTimelineFilepath", "setGeneralBiomFile",
                        "setGeneralBiomDir"],
    "advspectraloptions.cpp": ["setSpectraBinSpectra", "setSpectraFile",
                               "setSpectraFullSpectra"],
    "planarfitsettingsdialog.cpp": ["setPlanarFitFile"],
    "timelagsettingsdialog.cpp": ["setTimelagOptFile"],
    "pwbtimelagsettingsdialog.cpp": ["setTimelagOptFile"],
}


def read(name):
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


class InputsHaveTheButton(unittest.TestCase):
    def test_every_input_widget_enables_remote_browsing(self):
        for name, widgets in INPUT_WIDGETS.items():
            text = read(name)
            for w in widgets:
                with self.subTest(file=name, widget=w):
                    self.assertIn(f"{w}->setRemoteBrowseEnabled(true);", text)

    def test_output_widgets_do_not(self):
        for name, widgets in OUTPUT_WIDGETS.items():
            text = read(name)
            for w in widgets:
                with self.subTest(file=name, widget=w):
                    self.assertNotIn(f"{w}->setRemoteBrowseEnabled(true)", text)

    def test_tooltip(self):
        text = read("lineeditandbrowsewidget.cpp")
        self.assertIn('tr("Remote drive...")', text)
        self.assertIn('setToolTip(tr("Dropbox or Google Drive link"))', text)


class LinksSurviveStoring(unittest.TestCase):
    def test_setpath_keeps_a_link(self):
        text = read("lineeditandbrowsewidget.cpp")
        body = text[text.index("void LineEditAndBrowseWidget::setPath"):]
        body = body[:body.index("\n}\n") if "\n}\n" in body else body.index("\r\n}\r\n")]
        self.assertLess(body.index("RemoteSource::isRemote(path)"),
                        body.index("canonicalFilePath"))

    def test_setpath_refuses_a_link_without_the_button(self):
        text = read("lineeditandbrowsewidget.cpp")
        self.assertIn("if (!remoteEnabled_)", text)
        self.assertIn("Output locations must be local folders", text)

    def test_stores_do_not_clean_a_link(self):
        for name, setters in STORE_SITES.items():
            text = read(name)
            for s in setters:
                with self.subTest(file=name, setter=s):
                    self.assertNotRegex(text, rf"{s}\(QDir::cleanPath\(")
                    self.assertRegex(text, rf"{s}\(RemoteSource::cleanPath\(")


class ReadFilesAreDownloadedFirst(unittest.TestCase):
    def test_ancillary_file_tests_read_a_local_copy(self):
        for name in ("advspectraloptions.cpp", "planarfitsettingsdialog.cpp",
                     "timelagsettingsdialog.cpp", "pwbtimelagsettingsdialog.cpp"):
            with self.subTest(file=name):
                text = read(name)
                self.assertIn("RemoteSource::ensureLocal(fp, this)", text)
                self.assertNotIn("QFileInfo paramFilePath(fp);", text)

    def test_embedded_metadata_probe_downloads_the_archive(self):
        text = read("basicsettingspage.cpp")
        self.assertIn("RemoteSource::ensureLocal(listedZip, this)", text)

    def test_listing_goes_to_the_provider(self):
        text = read("fileutils.cpp")
        body = text[text.index("const QStringList FileUtils::getFiles"):]
        self.assertLess(body.index("RemoteSource::listFiles"), body.index("QtConcurrent::run"))


class MetadataIsCopiedNextToTheProject(unittest.TestCase):
    def test_remote_metadata_is_copied_before_it_is_opened(self):
        text = read("projectpage.cpp")
        body = text[text.index("void ProjectPage::metadataFileSelected"):]
        self.assertLess(body.index("copyRemoteMetadata(file_path)"),
                        body.index("dlIniDialog_->openFile"))

    def test_a_new_project_is_saved_first(self):
        text = read("mainwindow.cpp")
        self.assertIn("setProjectFileProvider", text)
        self.assertRegex(text, r"if \(newFlag_ && !fileSaveAs\(\)\) \{ return QString\(\); \}")


class DriveFollowsTheProject(unittest.TestCase):
    def test_opening_or_starting_a_project_resets_the_drive(self):
        text = read("mainwindow.cpp")
        self.assertIn("RemoteSource::adoptDriveFrom(", text)
        self.assertGreaterEqual(text.count("RemoteSource::clearCurrentDrive();"), 2)


class ProviderShapesMatchTheEngine(unittest.TestCase):
    """The requests must be the ones the engine makes, which were probed."""

    def test_google(self):
        text = read("remotesource.cpp")
        self.assertIn("embeddedfolderview", text)
        self.assertIn("drive.usercontent.google.com/download?id=%1", text)
        self.assertIn("confirm=t", text)

    def test_dropbox(self):
        text = read("remotesource.cpp")
        self.assertIn("list_shared_link_folder_entries", text)
        for field in ("is_xhr", "link_key", "secure_hash", "sub_path", "rlkey", "voucher"):
            with self.subTest(field=field):
                self.assertIn(f'QStringLiteral("{field}")', text)
        self.assertIn('c.name() == "t"', text)


if __name__ == "__main__":
    unittest.main()
