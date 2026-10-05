"""Browse opens where the field points, not where the field was last used.

Every browse widget keeps a dialogDir_ - "where this field was last browsed
from" - seeded once from its own QSettings key and overwritten after each
successful pick. setPath(), which is how every page's refresh() puts a
project's value into a field, never touched it, so the dialog opened at the
last used location even when the field was showing a perfectly good path read
from the project.

The start directory is now decided at click time, in the shared widgets, and
the order is the whole of it: the field's own location first, then the
location the field was given even if it has gone (the nearest folder above it
that still exists), then where that field was last browsed from, then the last
opened project's folder. Two things make it fragile in particular, and both
are pinned here: setPath() canonicalises, which leaves nothing behind for a
path that has gone, so the raw value has to be recorded separately; and a
shared-drive link must never reach QFileDialog.
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"


def read(name):
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


def body(text, marker):
    """The function that starts at marker, up to its closing brace."""
    start = text.index(marker)
    rest = text[start:]
    end = rest.index("\n}")
    return rest[:end]


class NearestExistingDirClimbs(unittest.TestCase):
    def test_it_is_declared_beside_existspath(self):
        header = read("fileutils.h")
        self.assertIn("QString nearestExistingDir(const QString& path);", header)

    def test_a_link_and_a_relative_value_are_refused(self):
        # a link is not a place on this filesystem; a relative value would be
        # resolved against wherever the application happens to be running
        impl = body(read("fileutils.cpp"), "QString FileUtils::nearestExistingDir")
        self.assertIn("RemoteSource::isRemote(path)", impl)
        self.assertIn("info.isRelative()", impl)

    def test_it_climbs_without_cdup(self):
        # QDir::cdUp answers false as soon as the directory above is missing
        # too, so it cannot climb out of a tree that has gone several levels
        # deep; QFileInfo::path is a string operation and always climbs
        impl = body(read("fileutils.cpp"), "QString FileUtils::nearestExistingDir")
        self.assertIn("existsPath(candidate)", impl)
        self.assertIn("QFileInfo(candidate).path()", impl)
        self.assertIn("parent == candidate", impl)
        # the comment above it may name cdUp; the code may not call it
        self.assertNotIn(".cdUp()", impl)


class TheFieldComesFirst(unittest.TestCase):
    def test_the_whole_fallback_order(self):
        impl = body(read("lineeditandbrowsewidget.cpp"),
                    "QString LineEditAndBrowseWidget::dialogStartDir")
        field = impl.index("nearestExistingDir")
        remembered = impl.index("dialogDir_")
        project_dir = impl.index("getSearchPathHint")
        self.assertLess(field, remembered,
                        "the field's own path must come before the last used location")
        self.assertLess(remembered, project_dir,
                        "the per-field location must come before the global hint")

    def test_the_candidate_is_the_path_never_the_displayed_text(self):
        # text() is the display name for a link ("Google Drive: site/2019")
        impl = body(read("lineeditandbrowsewidget.cpp"),
                    "QString LineEditAndBrowseWidget::dialogPathCandidate")
        self.assertIn("path()", impl)
        self.assertIn("RemoteSource::isRemote", impl)
        self.assertNotIn("text()", impl)

    def test_a_file_is_offered_only_while_it_is_there(self):
        # Qt answers a path that has gone with a directory of its own choosing
        impl = body(read("lineeditandbrowsewidget.cpp"),
                    "QString LineEditAndBrowseWidget::dialogStartFile")
        self.assertIn("isFile()", impl)


class TheHintSurvivesCanonicalisation(unittest.TestCase):
    def test_setpath_records_the_raw_value(self):
        text = read("lineeditandbrowsewidget.cpp")
        impl = body(text, "void LineEditAndBrowseWidget::setPath")
        refusal = impl.index("Output locations must be local folders")
        local = impl.index("remoteLink_.clear();")
        recorded = impl.index("dialogPathHint_ = path;")
        self.assertLess(refusal, local,
                        "a refused link must return before the local branch")
        self.assertLess(impl.index("canonicalFilePath"), recorded,
                        "the raw value is recorded even when canonicalising gave nothing")

    def test_clearing_the_field_clears_the_hint(self):
        impl = body(read("lineeditandbrowsewidget.cpp"),
                    "void LineEditAndBrowseWidget::clear")
        self.assertIn("dialogPathHint_.clear();", impl)


class BothDialogsUseIt(unittest.TestCase):
    def test_the_file_dialog_opens_at_the_file_or_its_folder(self):
        impl = body(read("filebrowsewidget.cpp"), "void FileBrowseWidget::onButtonClick")
        self.assertIn("startFile.isEmpty() ? dialogStartDir() : startFile", impl)
        self.assertNotIn("getSearchPathHint", impl)
        self.assertIn("setDialogWorkingDir(", impl)

    def test_the_folder_dialog_opens_at_the_folder(self):
        impl = body(read("dirbrowsewidget.cpp"), "void DirBrowseWidget::onButtonClick")
        self.assertIn("dialogStartDir()", impl)
        self.assertNotIn("getSearchPathHint", impl)
        self.assertIn("setDialogWorkingDir(", impl)

    def test_neither_hands_a_link_to_the_file_dialog(self):
        # path() may be an https:// link; the remote button is what browses it
        for name, marker in (("filebrowsewidget.cpp", "void FileBrowseWidget::onButtonClick"),
                             ("dirbrowsewidget.cpp", "void DirBrowseWidget::onButtonClick")):
            with self.subTest(widget=name):
                self.assertNotIn("path()", body(read(name), marker))


class EveryFieldRemembersItsOwn(unittest.TestCase):
    #: the fourteen path fields, and the key each remembers itself under
    FIELDS = [
        ("basicsettingspage.cpp", "datapathBrowse", "raw_data_dir"),
        ("basicsettingspage.cpp", "outpathBrowse", "output_dir"),
        ("projectpage.cpp", "metadataFileBrowse", "metadata_file"),
        ("projectpage.cpp", "dynamicMdFileBrowse", "dynamic_metadata_file"),
        ("projectpage.cpp", "biometExtFileBrowse", "external_biomet_file"),
        ("projectpage.cpp", "biometExtDirBrowse", "external_biomet_dir"),
        ("advspectraloptions.cpp", "binnedSpectraDirBrowse", "binned_cospectra_dir"),
        ("advspectraloptions.cpp", "spectraFileBrowse", "spectral_assessment_file"),
        ("advspectraloptions.cpp", "fullSpectraDirBrowse", "full_cospectra_dir"),
        ("planarfitsettingsdialog.cpp", "fileBrowse", "planar_fit_file"),
        ("timelagsettingsdialog.cpp", "fileBrowse", "timelag_file"),
        ("pwbtimelagsettingsdialog.cpp", "fileBrowse", "timelag_file"),
        ("createpackagedialog.cpp", "outpathBrowse", "package_output_dir"),
        # the head correction tables remembered nothing at all until this fix
        ("advprocessingoptions.cpp", "headCorrDirBrowse", "head_corr_dir"),
    ]

    def test_each_field_is_seeded_from_its_own_key(self):
        for name, widget, key in self.FIELDS:
            with self.subTest(field=widget, key=key):
                pattern = (r"%s->setDialogWorkingDir\(\s*WidgetUtils::getDialogPathHint\("
                           r"QStringLiteral\(\"%s\"\)\)\)" % (widget, key))
                self.assertRegex(read(name), pattern)

    def test_each_key_is_written_back_after_a_pick(self):
        everything = "".join(read(p.name) for p in sorted(SRC.glob("*.cpp")))
        for _, widget, key in self.FIELDS:
            with self.subTest(field=widget, key=key):
                self.assertRegex(
                    everything,
                    r'rememberDialogPath\(\s*QStringLiteral\("%s"\)' % key)


class APathThatHasGoneStillPointsSomewhere(unittest.TestCase):
    def test_the_two_fields_that_are_cleared_state_it_first(self):
        # refresh() clears the field AND wipes the project's value for these
        # two, so this is the one moment where a folder that has gone is still
        # known: the hint has to be taken between the two
        text = read("basicsettingspage.cpp")
        for getter, setter, field in (
                ("screenDataPath", "updateDataPath", "datapathBrowse"),
                ("generalOutPath", "updateOutPath", "outpathBrowse")):
            with self.subTest(field=field):
                slice_ = text[text.index("%s->clear();" % field):]
                slice_ = slice_[:slice_.index("%s(QString());" % setter)]
                self.assertIn("%s->setDialogPathHint(ecProject_->%s());"
                              % (field, getter), slice_)


if __name__ == "__main__":
    unittest.main()
