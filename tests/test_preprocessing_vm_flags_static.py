"""A pre-processing run leaves the Vickers and Mahrt filter off by default.

Choosing the run mode on Output Files switches "Filter (co)spectra according
to Vickers and Mahrt (1997) test results" off - when it is chosen, not in the
requirements re-applied on every refresh, which would undo the user's tick.
The Spectral Corrections page keeps the box enabled and shows what the
project holds; ticking it during pre-processing warns that the assessment may
fail.
"""

from pathlib import Path
import unittest

SRC = Path(__file__).resolve().parents[1] / "src"


def read(name):
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


def body(text, signature):
    """From the signature to the next function at column 0."""
    start = text.index(signature)
    end = text.find("\n}\n", start)
    return text[start:end]


class TheOutputFilesPage(unittest.TestCase):
    def setUp(self):
        self.cpp = read("advoutputoptions.cpp")

    def test_choosing_pre_processing_switches_the_filter_off(self):
        b = body(self.cpp, "void AdvOutputOptions::updateSpectralAssessmentCreationMode(bool checked)")
        self.assertIn("ecProject_->setSpectraUseVmFlags(0);", b)

    def test_refreshed_requirements_leave_the_filter_alone(self):
        b = body(self.cpp, "void AdvOutputOptions::applySpectralAssessmentCreationRequirements()")
        self.assertNotIn("SpectraUseVmFlags", b)


class TheSpectralCorrectionsPage(unittest.TestCase):
    def setUp(self):
        self.cpp = read("advspectraloptions.cpp")

    def test_box_follows_the_project_and_stays_enabled(self):
        start = self.cpp.index("QSignalBlocker vmBlocker(vmFlagsCheckBox);")
        branch = self.cpp[start:self.cpp.index("else if (productionRun)", start)]
        self.assertIn("vmFlagsCheckBox->setChecked(ecProject_->spectraUseVmFlags());", branch)
        self.assertIn("vmFlagsCheckBox->setEnabled(true);", branch)
        self.assertNotIn("setSpectraUseVmFlags", branch)

    def test_only_the_users_tick_warns(self):
        self.assertIn("connect(vmFlagsCheckBox, &QCheckBox::clicked,\n"
                      "            this, &AdvSpectralOptions::warnVmFlagsInPreprocessing);",
                      self.cpp)
        b = body(self.cpp, "void AdvSpectralOptions::warnVmFlagsInPreprocessing(bool checked)")
        self.assertIn("ecProject_->spectraFluxRunMode() != 1", b)
        self.assertIn("WidgetUtils::warning(this", b)
        self.assertIn("the spectral assessment may fail", b)


if __name__ == "__main__":
    unittest.main()
