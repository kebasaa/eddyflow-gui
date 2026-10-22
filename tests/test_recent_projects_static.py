"""The list of recent projects keeps 20, whatever the size of the window.

It kept 4, and a window of 1200x900 or smaller set the cap to 2: addRecent
trims to the cap of the moment and saves the result, so opening a project on
a laptop lost all but the two latest for good. The cap was also stored, and
read back on startup, so a new default alone would not have reached any
existing install.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"


def read(name):
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


def body(text, signature):
    """The braces of the function whose definition starts with signature."""
    start = text.index("{", text.index(signature))
    depth = 0
    for i in range(start, len(text)):
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        if depth == 0:
            return text[start:i + 1]
    raise ValueError(signature)


class TheListKeepsTwenty(unittest.TestCase):

    def test_the_default_is_twenty(self):
        self.assertIn("int recentnum = 20;", read("configstate.h"))

    def test_nothing_else_sets_the_cap(self):
        for path in sorted(SRC.glob("*.cpp")) + sorted(SRC.glob("*.h")):
            if path.name == "configstate.h":
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            self.assertIsNone(re.search(r"recentnum\s*=[^=]", text), path.name)

    def test_the_cap_is_not_stored(self):
        """A stored 4, or 2, from an older version would otherwise win."""
        self.assertNotIn("RECENTNUM", read("mainwindow.cpp"))
        self.assertNotIn("/recentnum", read("defs.h"))


class ResizingNeverShrinksIt(unittest.TestCase):

    def test_the_window_size_does_not_reach_the_list(self):
        page = read("mainwindow.cpp")
        for signature in ("void MainWindow::minimizeGui()",
                          "void MainWindow::maximizeGui()",
                          "void MainWindow::resizeEvent("):
            code = body(page, signature)
            self.assertNotIn("recentnum", code, signature)
            self.assertNotIn("recentfiles", code, signature)

    def test_the_small_welcome_page_keeps_every_row(self):
        code = body(read("welcomepage.cpp"),
                    "void WelcomePage::updateWelcomePage(")
        self.assertNotIn("recentListWidget", code)
        self.assertNotIn("recentfiles", code)


if __name__ == "__main__":
    unittest.main()
