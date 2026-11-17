"""power_of_two is a three-way choice: all samples, a power of two, or the
largest fast FFT length.

A power of two leaves out 9 % of 30 minutes at 20 Hz (32768 of 36000) and 43 %
at 32 Hz (32768 of 57600). The third choice, power_of_two=2, takes the largest
number of samples with no prime factor above 13, which the engine's FFTPACK
transforms within 1.5 times the power-of-two time and which leaves out at most
1 %. It sits beside the existing checkbox; ticking one unticks the other.

Pinned: the two boxes write 0, 1 and 2 and nothing else; loading shows the
value without writing it back; both are enabled and disabled together; and
the engine reads the same three values.
"""

import unittest
from pathlib import Path

GUI = Path(__file__).resolve().parents[1]
ENGINE = GUI.parent / "eddyflow-engine" / "src"
SRC = (GUI / "src" / "advspectraloptions.cpp").read_text(encoding="utf-8", errors="replace")
HDR = (GUI / "src" / "advspectraloptions.h").read_text(encoding="utf-8", errors="replace")


def body(signature):
    a = SRC.index(signature)
    return SRC[a:SRC.index("\n}", a)]


class TheThreeValues(unittest.TestCase):

    def test_the_boxes_write_zero_one_or_two(self):
        fn = body("void AdvSpectralOptions::updateFftLength()")
        self.assertIn("int value = 0;", fn)
        self.assertIn("value = 1;", fn)
        self.assertIn("value = 2;", fn)
        self.assertIn("ecProject_->setScreenlPowerOfTwo(value);", fn)

    def test_loading_shows_without_writing_back(self):
        fn = body("void AdvSpectralOptions::showFftLength(int value)")
        self.assertIn("QSignalBlocker blockPow2(fftCheckBox)", fn)
        self.assertIn("QSignalBlocker blockFast(fftFastLengthCheckBox)", fn)
        self.assertIn("fftCheckBox->setChecked(value == 1);", fn)
        self.assertIn("fftFastLengthCheckBox->setChecked(value == 2);", fn)
        self.assertIn("showFftLength(ecProject_->screenPowerOfTwo());", SRC)

    def test_default_values_reset_the_project_too(self):
        self.assertIn("ecProject_->setScreenlPowerOfTwo("
                      "ecProject_->defaultSettings.screenSetting.power_of_two);", SRC)

    def test_ticking_one_unticks_the_other(self):
        self.assertIn("fftFastLengthCheckBox->setChecked(false);", SRC)
        self.assertIn("fftCheckBox->setChecked(false);", SRC)
        self.assertNotIn("setScreenlPowerOfTwo(checked)", SRC)


class TheBoxesMoveTogether(unittest.TestCase):

    def test_declared_and_placed_beside_the_first(self):
        self.assertIn("QCheckBox* fftFastLengthCheckBox;", HDR)
        self.assertIn("settingsLayout->addWidget(fftFastLengthCheckBox, 8, 2, 1, 3);", SRC)

    def test_enabled_wherever_the_first_is(self):
        self.assertEqual(SRC.count("fftCheckBox->setEnabled("),
                         SRC.count("fftFastLengthCheckBox->setEnabled("))


@unittest.skipUnless(ENGINE.is_dir(), "engine repo not beside the GUI")
class TheEngineReadsTheSameValues(unittest.TestCase):

    def test_one_and_two(self):
        ini = (ENGINE / "src_rp" / "read_ini_rp.f90").read_text(encoding="utf-8", errors="replace")
        self.assertIn("RPsetup%power_of_two = SCTags(87)%value(1:1) == '1'", ini)
        self.assertIn("RPsetup%fft_friendly_len = SCTags(87)%value(1:1) == '2'", ini)


if __name__ == "__main__":
    unittest.main()
