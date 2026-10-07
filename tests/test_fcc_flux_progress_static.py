"""The progress bar moves through FCC's flux computation.

FCC's flux loop says "  Calculating fluxes for 13 May 2019" once a day. The run
page matched "Calculating fluxes for:", which the engine never writes, so the
bar stood still from the first day to the last - on a long record, many
minutes of a bar that seemed to have stopped.
"""

import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
RUNPAGE = (GUI_ROOT / "src/runpage.cpp").read_text(encoding="utf-8").replace("\r\n", "\n")
HEADER = (GUI_ROOT / "src/runpage.h").read_text(encoding="utf-8").replace("\r\n", "\n")


class FccFluxDays(unittest.TestCase):

    def test_the_engine_s_own_line_is_matched(self):
        self.assertNotIn('QByteArrayLiteral("Calculating fluxes for:")', RUNPAGE)
        self.assertIn('QByteArrayLiteral("Calculating fluxes for ")', RUNPAGE)

    def test_the_days_of_the_range_are_counted_once_per_run(self):
        i = RUNPAGE.index('QByteArrayLiteral("Calculating fluxes for ")')
        block = RUNPAGE[i:RUNPAGE.index("return;", i)]
        self.assertIn("if (!fccFluxDays_)", block)
        self.assertIn("dStart.daysTo(dEnd)) + 1", block)
        self.assertIn("ecProject_->generalSubset()", block)
        self.assertIn("bool fccFluxDays_ = false;", HEADER)

    def test_a_new_fcc_session_starts_counting_again(self):
        i = RUNPAGE.index('QByteArrayLiteral("Starting flux computation and correction")')
        self.assertIn("fccFluxDays_ = false;", RUNPAGE[i:RUNPAGE.index("return;", i)])


if __name__ == "__main__":
    unittest.main()
