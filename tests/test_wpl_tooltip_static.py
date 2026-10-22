"""The WPL tooltip says how the correction is actually decided.

The engine decides it gas by gas, from the measure type in the Raw File
Description: a dry mixing ratio is never corrected, a mole fraction or a molar
density is. So an analyser that reports dry mixing ratios - the MIRO's gases -
is not corrected twice, and nothing per instrument is needed to prevent it.
The tooltip used to say the second half and not what follows from it, and not
what unticking does since 8.1.3: every gas keeps its uncorrected flux, as in
EddyPro, where before the gases were corrected regardless.

Its question mark also pointed at a host and page name the documentation site
no longer has.
"""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def tooltip():
    page = (ROOT / "src" / "advprocessingoptions.cpp").read_text(
        encoding="utf-8", errors="replace")
    start = page.index("wplCheckBox->setToolTip(")
    return page[start:page.index("wplCheckBox->setText(", start)]


def question_mark():
    page = (ROOT / "src" / "advprocessingoptions.cpp").read_text(
        encoding="utf-8", errors="replace")
    start = page.index("wplCheckBox->setQuestionMark(")
    return page[start:page.index("\n", start)]


class TheTooltipStatesTheRule(unittest.TestCase):

    def setUp(self):
        self.text = tooltip()

    def test_it_is_decided_per_gas_from_the_measure_type(self):
        self.assertIn("gas by gas", self.text)
        self.assertIn("measure type", self.text)

    def test_mixing_ratios_are_never_corrected(self):
        self.assertIn("<b>Mixing ratios</b>", self.text)
        self.assertIn("never corrected, ticked or not", self.text)
        self.assertIn("not corrected twice", self.text)

    def test_wet_water_is_still_corrected(self):
        """Turning an instrument off wholesale would get this wrong."""
        self.assertIn("mole fraction of wet air, is still corrected", self.text)

    def test_each_hygrometer_and_its_gases(self):
        self.assertIn("its own path type", self.text)
        self.assertIn("hygrometer its record names", self.text)

    def test_what_unticking_does(self):
        self.assertIn("<b>Unticked</b>, no gas is density-corrected", self.text)

    def test_the_advice_is_the_measure_type_not_the_box(self):
        self.assertIn("measure type it really reports", self.text)


class TheQuestionMarkReachesThePage(unittest.TestCase):

    def test_the_documentation_site_and_page(self):
        link = question_mark()
        self.assertIn("https://kebasaa.github.io/eddyflow-documentation/"
                      "topics/converting-to-mixing-ratio/", link)
        self.assertNotIn("keba_saa", link)


if __name__ == "__main__":
    unittest.main()
