""""Automatically configure spectral assessment after this run" is an output
option, so it lives on Output Files.

It changes nothing in the run that has it: when the run makes an on-the-fly
spectral assessment, the engine writes the flux limits and transfer-function
sources it recommends into the output project, processing_*.eddyflow. It used
to sit on Spectral Analysis and Corrections among the correction options; it
now sits under Assessment File Outputs, beside the run modes that decide
whether an assessment is made.

Pinned: it is gone from the spectral page; on Output Files it is in the
assessment group, writes automatic_spectra_config through the project, and is
set both when a project is loaded and when the page is reset.
"""

import unittest
from pathlib import Path

SRC = Path(__file__).resolve().parents[1] / "src"
SPECTRAL = (SRC / "advspectraloptions.cpp").read_text(encoding="utf-8", errors="replace")
SPECTRAL_H = (SRC / "advspectraloptions.h").read_text(encoding="utf-8", errors="replace")
OUTPUT = (SRC / "advoutputoptions.cpp").read_text(encoding="utf-8", errors="replace")
OUTPUT_H = (SRC / "advoutputoptions.h").read_text(encoding="utf-8", errors="replace")


def body(signature):
    a = OUTPUT.index(signature)
    return OUTPUT[a:OUTPUT.index("\n}", a)]


class ItLeftTheSpectralPage(unittest.TestCase):

    def test_no_trace_of_it_there(self):
        self.assertNotIn("automaticSpectraConfig", SPECTRAL)
        self.assertNotIn("automaticSpectraConfig", SPECTRAL_H)
        self.assertNotIn("setSpectraAutomaticConfig", SPECTRAL)


class ItIsAnOutputOption(unittest.TestCase):

    def test_same_label_on_output_files(self):
        self.assertIn("QCheckBox* automaticSpectraConfigCheckBox;", OUTPUT_H)
        self.assertIn('tr("Automatically configure spectral assessment after this run")', OUTPUT)

    def test_in_the_assessment_group(self):
        self.assertIn("assessmentFileOutputsLayout->addWidget(automaticSpectraConfigContainer);", OUTPUT)

    def test_it_writes_the_same_key(self):
        self.assertIn("{ ecProject_->setSpectraAutomaticConfig(checked); }", OUTPUT)

    def test_loaded_and_reset(self):
        self.assertIn("automaticSpectraConfigCheckBox->setChecked(ecProject_->spectraAutomaticConfig());",
                      body("void AdvOutputOptions::refresh()"))
        self.assertIn("automaticSpectraConfigCheckBox->setChecked("
                      "ecProject_->defaultSettings.spectraSettings.automatic_spectra_config);",
                      body("void AdvOutputOptions::reset()"))


if __name__ == "__main__":
    unittest.main()
