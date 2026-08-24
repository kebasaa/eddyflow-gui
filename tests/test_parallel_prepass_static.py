"""The parallel pre-pass tickbox is a machine preference, not a project one.

`eddyflow_rp` can split its planar-fit and time-lag pre-passes across worker
processes, and the switch that asks for it is `-j`. How many cores this
computer should hand the engine has nothing to do with the site being
processed, so the tickbox writes into the application preferences rather than
into the `.eddyflow` - which also means no new ini key, no move of the
generated tag tables, and nothing to add to the four save/load sites in
`ecproject.cpp`.

Two details are load-bearing.

**`-j` must be passed in BOTH states.** The engine's own default, with no `-j`
at all, is to use every core. Passing the switch only when the box is ticked
would therefore leave an unticked box doing nothing, which is the opposite of
what it says.

**It must hang off `clicked`, not `toggled`.** `refresh()` blocks the
project's signals, not the widgets', so a `setChecked` there still emits
`toggled` - and the preference would be rewritten every time a project was
opened.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


PAGE = read("src/advprocessingoptions.cpp")
PAGE_H = read("src/advprocessingoptions.h")
STATE = read("src/configstate.h")
DEFS = read("src/defs.h")
MAIN = read("src/mainwindow.cpp")
PROJECT = read("src/ecproject.cpp")


class ThePreferenceIsStoredWithTheApplication(unittest.TestCase):

    def test_it_lives_in_the_general_config_state(self):
        block = STATE[STATE.index("struct GenConfigState"):
                      STATE.index("struct ProjConfigState")]
        self.assertIn("bool parallelPrepass = false;", block)

    def test_it_ships_off(self):
        """Until the serial-versus-parallel equivalence runs are signed off."""
        self.assertIn("bool parallelPrepass = false;", STATE)
        self.assertNotIn("bool parallelPrepass = true;", STATE)

    def test_it_has_a_settings_key(self):
        self.assertIn("CONF_GEN_PARALLEL_PREPASS", DEFS)

    def test_it_is_read_and_written(self):
        self.assertIn("settings.value(Defs::CONF_GEN_PARALLEL_PREPASS", MAIN)
        self.assertIn("settings.setValue(Defs::CONF_GEN_PARALLEL_PREPASS", MAIN)

    def test_it_is_not_a_project_setting(self):
        """No ini key, so nothing in the project save/load path knows of it."""
        self.assertNotIn("parallelPrepass", PROJECT)
        self.assertNotIn("parallel_prepass", PROJECT)


class TheSwitchIsPassedInBothStates(unittest.TestCase):
    """Otherwise an unticked box falls through to the engine's own default."""

    def test_the_switch_is_passed_twice(self):
        """Once for the express run, once for the advanced one."""
        self.assertEqual(MAIN.count('args << QStringLiteral("-j");'), 2)

    def test_each_one_chooses_between_auto_and_serial(self):
        for match in re.finditer(r'args << QStringLiteral\("-j"\);', MAIN):
            chunk = MAIN[match.end():match.end() + 260]
            self.assertIn("configState_.general.parallelPrepass", chunk)
            self.assertIn('QStringLiteral("0")', chunk)
            self.assertIn('QStringLiteral("1")', chunk)

    def test_it_goes_only_to_the_raw_processing_engine(self):
        """FCC has no pre-pass, and metadata retrieval never reaches one."""
        for match in re.finditer(r'args << QStringLiteral\("-j"\);', MAIN):
            chunk = MAIN[match.end():match.end() + 900]
            self.assertIn("Defs::ENGINE_RP", MAIN[:match.start()][-2500:],
                          "-j must sit in a block that launches eddyflow_rp")
            self.assertNotIn("ENGINE_FCC", chunk)

    def test_the_switch_precedes_the_project_path(self):
        """The engine reads both from one list; the path goes last."""
        for match in re.finditer(r'args << QStringLiteral\("-j"\);', MAIN):
            chunk = MAIN[match.end():match.end() + 400]
            self.assertRegex(chunk, r"args << projFilePath\d?;")


class TheTickboxIsOnTheProcessingOptionsPage(unittest.TestCase):

    def test_it_exists(self):
        self.assertIn("RichTextCheckBox* parallelPrepassCheckBox;", PAGE_H)
        self.assertIn("parallelPrepassCheckBox = new RichTextCheckBox;", PAGE)

    def test_it_sits_under_other_options(self):
        """Below the controls that change the numbers, not among them."""
        other = PAGE.index('auto qcTitle = new QLabel(tr("Other options"));')
        self.assertLess(other, PAGE.index("parallelPrepassCheckBox = new"))
        row = re.search(r"settingsLayout->addWidget\(parallelPrepassCheckBox, (\d+),",
                        PAGE)
        self.assertIsNotNone(row, "the tickbox is never added to the layout")
        cec = re.search(r"settingsLayout->addWidget\(cecCheckBox, (\d+),", PAGE)
        self.assertGreater(int(row.group(1)), int(cec.group(1)))

    def test_the_row_stretch_moved_below_it(self):
        """A stretch on an occupied row lets that row absorb the slack."""
        row = int(re.search(
            r"settingsLayout->addWidget\(parallelPrepassCheckBox, (\d+),",
            PAGE).group(1))
        stretch = int(re.search(r"settingsLayout->setRowStretch\((\d+), 1\);",
                                PAGE).group(1))
        self.assertGreater(stretch, row)

    @staticmethod
    def connection():
        """Just the one connect statement, not whatever follows it."""
        i = PAGE.index("connect(parallelPrepassCheckBox")
        return PAGE[i:PAGE.index("});", i) + 3]

    def test_it_writes_the_preference_on_a_real_click_only(self):
        """refresh() blocks the project's signals, not the widget's."""
        chunk = self.connection()
        self.assertIn("&RichTextCheckBox::clicked", chunk)
        self.assertNotIn("::toggled", chunk)
        self.assertIn("configState_->general.parallelPrepass", chunk)

    def test_it_does_not_dirty_the_project(self):
        self.assertNotIn("ecProject_", self.connection())

    def test_it_shows_the_stored_preference(self):
        self.assertIn(
            "parallelPrepassCheckBox->setChecked(configState_->general.parallelPrepass);",
            PAGE)

    def test_the_tooltip_says_the_results_do_not_change(self):
        i = PAGE.index("parallelPrepassCheckBox->setToolTip")
        tip = PAGE[i:PAGE.index(";", i)]
        self.assertIn("not change", tip.replace("<b>", "").replace("</b>", ""))

    def test_the_tooltip_says_it_is_not_saved_with_the_project(self):
        i = PAGE.index("parallelPrepassCheckBox->setToolTip")
        tip = PAGE[i:PAGE.index(";", i)]
        self.assertIn("this computer", tip)
        self.assertIn("project file", tip)


if __name__ == "__main__":
    unittest.main()
