"""A pre-processing run filters (co)spectra by the Vickers and Mahrt tests.

Both places that set up a pre-processing run - the run mode on Output Files
and the Spectral Corrections page's refresh - switch the filter on and leave
its checkbox greyed out. The engine's filter only drops a period a test
flagged 1, and a test that did not run flags 9, so selecting pre-processing
names the four tests the filter reads when any of them is off.
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

    def test_requirements_switch_the_filter_on(self):
        b = body(self.cpp, "void AdvOutputOptions::applySpectralAssessmentCreationRequirements()")
        self.assertIn("ecProject_->setSpectraUseVmFlags(1);", b)
        self.assertNotIn("setSpectraUseVmFlags(0)", b)

    def test_suggestion_after_the_quiet_section(self):
        b = body(self.cpp, "void AdvOutputOptions::updateSpectralAssessmentCreationMode(bool checked)")
        self.assertLess(b.index("applyRunModeRequirements("),
                        b.index("suggestVickersMahrtTests();"))

    def test_suggestion_names_the_four_tests_the_filter_reads(self):
        b = body(self.cpp, "void AdvOutputOptions::suggestVickersMahrtTests()")
        for getter in ("screenTestSr()", "screenTestDo()",
                       "screenTestSk()", "screenTestDs()"):
            self.assertIn(getter, b)
        self.assertIn("WidgetUtils::information(this", b)
        self.assertIn("if (allOn)", b)


class TheSpectralCorrectionsPage(unittest.TestCase):
    def test_checkbox_ticked_and_locked_in_pre_processing(self):
        cpp = read("advspectraloptions.cpp")
        start = cpp.index("vmFlagsCheckBox->setChecked(true);")
        branch = cpp[cpp.rindex("QSignalBlocker vmBlocker(vmFlagsCheckBox);", 0, start):
                     cpp.index("else if (productionRun)")]
        self.assertIn("vmFlagsCheckBox->setChecked(true);", branch)
        self.assertIn("vmFlagsCheckBox->setEnabled(false);", branch)
        self.assertNotIn("setSpectraUseVmFlags(0)", cpp)


if __name__ == "__main__":
    unittest.main()
