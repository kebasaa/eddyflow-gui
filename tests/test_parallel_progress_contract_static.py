"""The run page follows a production pass the engine has split across workers.

After its first fully processed half-hour the engine may hand the rest of the
range to worker processes. From then on the parent prints no more
"processing new flux averaging period" lines - only the pieces its workers
finish - so a progress bar that counts periods stops at the first one and sits
there until the run ends. The page reads the piece counts instead.

These are the engine's own lines, matched as text, so both sides are pinned:
a reworded message in the engine would otherwise freeze the bar again without
anything failing.
"""

import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_MAIN = ENGINE_ROOT / "src" / "src_rp" / "eddyflow-rp_main.f90"
ENGINE_POOL = ENGINE_ROOT / "src" / "src_rp" / "prepass_parallel.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


PAGE = read(GUI_ROOT / "src" / "runpage.cpp")
HEADER = read(GUI_ROOT / "src" / "runpage.h")

engine_available = ENGINE_MAIN.is_file() and ENGINE_POOL.is_file()


class ThePageReadsThePieces(unittest.TestCase):

    def test_it_recognises_the_split_and_its_phases(self):
        for text in ("Splitting the production pass across",
                     "PWB time lags: each piece is first read",
                     "Waiting for the workers:",
                     " pieces done."):
            self.assertIn('QByteArrayLiteral("%s")' % text, PAGE)

    def test_it_starts_afresh_with_every_run(self):
        i = PAGE.index('QByteArrayLiteral("Start raw data processing")')
        block = PAGE[i:PAGE.index("return;", i)]
        self.assertIn("prodSplit_ = false;", block)

    def test_the_bar_never_runs_backwards(self):
        self.assertIn("if (value > progressValue_)", PAGE)

    def test_its_state_is_per_page(self):
        for member in ("bool prodSplit_", "bool prodPwb_", "int prodPhase_", "int prodBaseValue_"):
            self.assertIn(member, HEADER)


@unittest.skipUnless(engine_available, "engine sources not next to the GUI")
class TheEngineSaysWhatThePageReads(unittest.TestCase):

    def test_the_split_announcement(self):
        self.assertIn("'  Splitting the ' // trim(what) // ' across '", read(ENGINE_POOL))
        self.assertIn("'production pass'", read(ENGINE_MAIN))

    def test_the_pwb_phases(self):
        self.assertIn("PWB time lags: each piece is first read", read(ENGINE_MAIN))

    def test_the_piece_counts(self):
        pool = read(ENGINE_POOL)
        self.assertIn("call LogSay('  Waiting for the workers:')", pool)
        self.assertIn("// ' pieces done.')", pool)
        self.assertIn("' of ' &", pool)


if __name__ == "__main__":
    unittest.main()
