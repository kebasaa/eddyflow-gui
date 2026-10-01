"""A page refreshing itself does not interrupt anybody.

Clicking a run mode on Output Files opened a stack of modal warnings. Each
setting the mode requires announces itself, every announcement refreshes every
page, and each page writes its own widgets on the way - so warnings reachable
from a refresh opened once per pass.

Blocking the project's signals, which every refresh() already does, cannot fix
this: it silences the PROJECT, never the page's own widgets, and the widgets'
handlers are what reach the warnings. So warning() and information() go quiet
while settings are being applied on the user's behalf, and say in the log what
they would have shown. critical(), the questions and requestToSave() stay as
they are: each either reports a failure that stopped something or asks
something only the user can answer.

Held here as well: the four places that did the needless work the guard would
otherwise only hide - the metadata file re-read on every announcement, the
spectral page's own handlers fired by its refresh, the Massman warning re-armed
on each pass, and a model reset announced back to the project that caused it.
"""

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
    return rest[:rest.index("\n}")]


class TheGuardExists(unittest.TestCase):
    def test_it_is_counted_not_a_flag(self):
        # one refresh reaches another, so the guard has to nest
        header = read("widget_utils.h")
        self.assertIn("class QuietWarnings", header)
        self.assertIn("static bool active();", header)
        self.assertIn("static int depth_;", header)
        impl = read("widget_utils.cpp")
        self.assertIn("++depth_;", impl)
        self.assertIn("--depth_;", impl)
        self.assertIn("return depth_ > 0;", impl)

    def test_warning_and_information_go_to_the_log(self):
        text = read("widget_utils.cpp")
        for marker in ("bool WidgetUtils::information",
                       "void WidgetUtils::warning"):
            with self.subTest(dialog=marker):
                impl = body(text, marker)
                self.assertIn("if (QuietWarnings::active())", impl)
                self.assertIn("logQuietly(title, text, infoText);", impl)
                # and before the window is built, or it would still open
                self.assertLess(impl.index("QuietWarnings::active()"),
                                impl.index("std::make_unique<QMessageBox>"))
        self.assertIn("qWarning()", body(text, "static void logQuietly"))

    def test_the_ones_that_must_still_be_seen_are_untouched(self):
        text = read("widget_utils.cpp")
        for marker in ("void WidgetUtils::critical",
                       "bool WidgetUtils::yesNoQuestion",
                       "QMessageBox::ButtonRole WidgetUtils::requestToSave",
                       "bool WidgetUtils::okToOverwrite"):
            with self.subTest(dialog=marker):
                self.assertNotIn("QuietWarnings", body(text, marker))


class EveryRefreshTakesIt(unittest.TestCase):
    #: the page functions that pour the project into their widgets
    REFRESHES = [
        ("advoutputoptions.cpp", "void AdvOutputOptions::reset()"),
        ("advoutputoptions.cpp", "void AdvOutputOptions::refresh()"),
        ("advprocessingoptions.cpp", "void AdvProcessingOptions::reset()"),
        ("advprocessingoptions.cpp", "void AdvProcessingOptions::refresh()"),
        ("advspectraloptions.cpp", "void AdvSpectralOptions::reset()"),
        ("advspectraloptions.cpp", "void AdvSpectralOptions::partialRefresh()"),
        ("advspectraloptions.cpp", "void AdvSpectralOptions::refresh()"),
        ("advstatisticaloptions.cpp", "void AdvStatisticalOptions::reset()"),
        ("advstatisticaloptions.cpp", "void AdvStatisticalOptions::refresh()"),
        ("basicsettingspage.cpp", "void BasicSettingsPage::reset()"),
        ("basicsettingspage.cpp", "void BasicSettingsPage::refresh()"),
        ("basicsettingspage.cpp", "void BasicSettingsPage::partialRefresh()"),
        ("projectpage.cpp", "void ProjectPage::reset()"),
        ("projectpage.cpp", "void ProjectPage::refresh()"),
    ]

    def test_each_one_declares_the_guard_before_it_starts_writing(self):
        for name, marker in self.REFRESHES:
            with self.subTest(function=marker):
                impl = body(read(name), marker)
                self.assertIn("const WidgetUtils::QuietWarnings quietWhileRefreshing;",
                              impl)
                self.assertLess(impl.index("QuietWarnings"),
                                impl.index("ecProject_->blockSignals(true);"))

    def test_the_run_mode_is_applied_quietly(self):
        impl = body(read("advoutputoptions.cpp"),
                    "void AdvOutputOptions::applyRunModeRequirements")
        self.assertIn("const WidgetUtils::QuietWarnings quietWhileApplying;", impl)

    def test_the_users_own_request_still_answers_back(self):
        # this one runs before the mode is applied, and its warnings are the
        # whole point: they say why the mode cannot be had
        impl = body(read("advoutputoptions.cpp"),
                    "bool AdvOutputOptions::validateSpectralAssessmentCreationRequest")
        self.assertNotIn("QuietWarnings", impl)
        self.assertIn("WidgetUtils::warning", impl)


class TheMetadataIsReReadOnlyWhenItChanges(unittest.TestCase):
    def test_the_comparison_is_the_one_the_field_makes(self):
        # the widget keeps the canonical spelling, so a plain string compare
        # against the project's value re-opened the same file every time
        text = read("projectpage.cpp")
        self.assertIn("static bool sameMetadataFile", text)
        impl = body(text, "void ProjectPage::refreshMetadata")
        self.assertIn("sameMetadataFile(mdFile, metadataFileBrowse->path())", impl)
        self.assertIn("sameMetadataFile(mdFile, lastMetadataRead_)", impl)
        self.assertNotIn("mdFile != metadataFileBrowse->path()", impl)
        self.assertLess(impl.index("sameMetadataFile"), impl.index("openFile"))

    def test_a_link_is_only_ever_itself(self):
        impl = body(read("projectpage.cpp"), "static bool sameMetadataFile")
        self.assertIn("RemoteSource::isRemote", impl)
        self.assertIn("canonicalFilePath", impl)

    def test_what_was_read_is_remembered(self):
        self.assertIn("QString lastMetadataRead_;", read("projectpage.h"))

    def test_the_suffix_box_is_validated_when_the_user_has_finished(self):
        # the same shape the Output Files error label was fixed out of
        text = read("projectpage.cpp")
        self.assertNotIn("&QComboBox::currentTextChanged,\n            this, &ProjectPage::updateExtDirSuffix",
                         text)
        self.assertNotIn("&QComboBox::editTextChanged,\n            this, &ProjectPage::updateExtDirSuffix",
                         text)
        self.assertIn("&QLineEdit::editingFinished", text)
        self.assertIn("&QComboBox::activated", text)


class TheSpectralPageStopsDrivingItsOwnHandlers(unittest.TestCase):
    def test_the_method_widgets_are_silenced_and_set_in_order(self):
        # the check box's handler writes setHfMethod(hfMethCombo->currentIndex()),
        # so setting the box first stored the method from a stale combo - and
        # the repair that followed was a real change, which announced itself
        impl = body(read("advspectraloptions.cpp"), "void AdvSpectralOptions::refresh()")
        self.assertIn("const QSignalBlocker methodBlocker(hfMethodCheck);", impl)
        self.assertIn("const QSignalBlocker comboBlocker(hfMethCombo);", impl)
        self.assertLess(impl.index("hfMethCombo->setCurrentIndex"),
                        impl.index("hfMethodCheck->setChecked"))

    def test_the_massman_warning_is_shown_once_a_session(self):
        text = read("advspectraloptions.cpp")
        self.assertIn("massmanFallbackWarningShown_ = true;", text)
        self.assertNotIn("massmanFallbackWarningShown_ = false;", text)


class AModelResetDoesNotAnnounceTheChangeThatCausedIt(unittest.TestCase):
    def test_the_reset_no_longer_goes_straight_to_the_project(self):
        text = read("planarfitsettingsdialog.cpp")
        self.assertNotIn("&AngleTableModel::modelReset,\n            ecProject_, &EcProject::updateInfo",
                         text)
        self.assertIn("this, &PlanarFitSettingsDialog::onAngleModelReset", text)
        self.assertIn("&PlanarFitSettingsDialog::angleTableChanged,\n            ecProject_, &EcProject::updateInfo",
                      text)

    def test_a_flush_driven_by_the_project_is_not_announced_back(self):
        text = read("planarfitsettingsdialog.cpp")
        update = body(text, "void PlanarFitSettingsDialog::updateModel")
        self.assertIn("flushingFromProjectChange_", update)
        self.assertLess(update.index("flushingFromProjectChange_"),
                        update.index("angleTableModel_->flush();"))
        on_reset = body(text, "void PlanarFitSettingsDialog::onAngleModelReset")
        self.assertIn("if (flushingFromProjectChange_) { return; }", on_reset)
        self.assertIn("emit angleTableChanged();", on_reset)

    def test_the_view_still_hears_the_reset(self):
        # only the project is spared; blocking the model would leave the
        # table and the pie stale
        update = body(read("planarfitsettingsdialog.cpp"),
                      "void PlanarFitSettingsDialog::updateModel")
        self.assertNotIn("QSignalBlocker", update)


if __name__ == "__main__":
    unittest.main()
