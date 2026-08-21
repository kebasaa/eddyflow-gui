"""Conditional Eddy Covariance needs the density correction, and says so.

The engine converts the raw series to mixing ratios only when WPL is on. The
partition sorts each air parcel into an octant by the SIGN of its water and
carbon dioxide fluctuation, and in an uncorrected molar density from an
open-path analyser part of that fluctuation is the air expanding rather than
the gas arriving - large enough to reverse the sign and put the parcel in the
wrong octant. Zahn et al. (2022) require the fluctuations themselves to carry
the correction, separately from the totals.

So the page couples the two: ticking the partition switches the correction on,
turning the correction back off warns, and a triangle beside it stays lit while
the project is in that state. The correction is still the user's to switch off -
a closed-path analyser reporting mixing ratios is unaffected, and the engine
repeats the point as Warning(114) at run time.

The load-bearing detail is WHICH SIGNAL each of those hangs off. `refresh()`
blocks the project's signals, not the widgets', so `setChecked` there still
emits `toggled`. Anything with a side effect the user did not ask for - the
auto-enable, the dialog - must therefore hang off `clicked`, which
RichTextCheckBox emits only for a real interaction. The triangle is the
exception and deliberately uses `toggled`: it has no side effect, and a project
saved in the inconsistent state should open with it already lit.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def body(source, signature):
    """One function's body, so a match cannot come from elsewhere in the file."""
    start = source.index(signature)
    return source[start:source.index("\n}\n", start)]


class TheTwoSettingsAreCoupled(unittest.TestCase):
    def setUp(self):
        self.page = read("src/advprocessingoptions.cpp")

    def test_ticking_the_partition_switches_the_correction_on(self):
        self.assertIn("wplCheckBox->setChecked(true);", self.page)
        #> Guarded on the partition being the thing that was just ticked, so it
        #> cannot switch the correction on when the user unticked something.
        self.assertIn("if (cecCheckBox->isChecked() && !wplCheckBox->isChecked())",
                      self.page)

    def test_the_auto_enable_hangs_off_clicked_not_toggled(self):
        """Otherwise it fires while a project is loading and silently rewrites
        a setting in a file the user only opened."""
        self.assertIn("connect(cecCheckBox, &RichTextCheckBox::clicked", self.page)
        block = self.page[self.page.index("connect(cecCheckBox, &RichTextCheckBox::clicked"):]
        block = block[:block.index("connect(wplCheckBox")]
        self.assertIn("wplCheckBox->setChecked(true);", block)
        self.assertNotIn("toggled", block)

    def test_the_warning_hangs_off_clicked_not_toggled(self):
        self.assertIn("connect(wplCheckBox, &RichTextCheckBox::clicked,\n"
                      "            this, &AdvProcessingOptions::warnWplOffWithCec);",
                      self.page)
        #> And is not ALSO wired to toggled, which would reintroduce the modal
        #> dialog on project load by the back door.
        self.assertNotIn("toggled,\n            this, &AdvProcessingOptions::warnWplOffWithCec",
                         self.page)

    def test_the_warning_is_silent_unless_the_partition_is_on(self):
        """Turning the correction off on a project with no partition is an
        ordinary choice and must not be nagged about."""
        block = body(self.page, "void AdvProcessingOptions::warnWplOffWithCec()")
        self.assertIn("if (wplCheckBox->isChecked()) { return; }", block)
        self.assertIn("if (!cecCheckBox->isChecked()) { return; }", block)
        for guard in ("if (wplCheckBox->isChecked()) { return; }",
                      "if (!cecCheckBox->isChecked()) { return; }"):
            self.assertLess(block.index(guard), block.index("WidgetUtils::warning"),
                            "the warning is reachable without %s" % guard)

    def test_the_warning_leaves_the_setting_where_the_user_put_it(self):
        """A warning, not a veto - the point is to inform, not to overrule."""
        block = body(self.page, "void AdvProcessingOptions::warnWplOffWithCec()")
        self.assertNotIn("setChecked", block)

    def test_the_partition_tooltip_says_it_switches_the_correction_on(self):
        block = self.page[self.page.index("cecCheckBox = new RichTextCheckBox;"):]
        block = block[:block.index("cecSettingsButton = new QPushButton")]
        self.assertIn("WPL", block)
        self.assertIn("Ticking this also switches on", block)


class TheTriangleMarksTheInconsistentState(unittest.TestCase):
    def setUp(self):
        self.page = read("src/advprocessingoptions.cpp")

    def test_it_needs_both_halves_of_the_condition(self):
        """Lit when the partition is on AND the correction is off. Lighting it
        whenever the partition is on would leave it on in the correct
        configuration, and an icon that is usually lit is one nobody reads."""
        block = body(self.page, "void AdvProcessingOptions::updateWplCecWarning()")
        self.assertIn("cecCheckBox->isChecked()", block)
        self.assertIn("!wplCheckBox->isChecked()", block)
        self.assertIn("setVisible", block)

    def test_it_starts_hidden(self):
        self.assertIn("wplWarningLabel->hide();", self.page)

    def test_a_project_saved_that_way_opens_with_it_lit(self):
        """The one place this must fire that the two toggles cannot reach:
        setChecked to an unchanged value emits nothing, so without an explicit
        call the icon keeps the previous project's state."""
        block = body(self.page, "void AdvProcessingOptions::refresh()")
        self.assertIn("updateWplCecWarning();", block)

    def test_both_toggles_keep_it_in_step(self):
        self.assertIn("connect(wplCheckBox, &RichTextCheckBox::toggled,\n"
                      "            this, &AdvProcessingOptions::updateWplCecWarning);",
                      self.page)
        #> The partition side goes through updateCecMeth_1 rather than a
        #> connection of its own, because that runs after
        #> updateCecAvailability - which can force the box back off under a
        #> QSignalBlocker, emitting nothing.
        block = body(self.page, "void AdvProcessingOptions::updateCecMeth_1(bool b)")
        self.assertIn("updateWplCecWarning();", block)
        self.assertLess(block.index("updateCecAvailability();"),
                        block.index("updateWplCecWarning();"))

    def test_it_uses_the_icon_the_pairing_table_uses(self):
        self.assertIn("QStyle::SP_MessageBoxWarning", self.page)
        self.assertIn("QStyle::SP_MessageBoxWarning",
                      read("src/cecpairmodel.cpp"),
                      "the pairing table's warning icon moved, so these two no "
                      "longer agree on what a warning looks like")

    def test_it_sits_in_the_checkbox_own_cell(self):
        """Column 0 is as wide as its widest widget, so an icon in a
        neighbouring grid cell strands itself far to the right of the text it
        belongs to."""
        self.assertIn("wplBox->addWidget(wplCheckBox);", self.page)
        self.assertIn("wplBox->addWidget(wplWarningLabel);", self.page)
        self.assertIn("settingsLayout->addLayout(wplBox, 10, 0);", self.page)
        self.assertNotIn("settingsLayout->addWidget(wplCheckBox, 10, 0);", self.page)


class TheUnavailableTooltipDoesNotStick(unittest.TestCase):
    """updateCecAvailability swaps the tooltip for an "unavailable" one when the
    site lacks a channel, and has to be able to put the real one back.

    It used to restore `cecCheckBox->toolTip()` - which, once the box had been
    unavailable once, WAS the unavailable text. The real tooltip never came back
    for the rest of the session.
    """

    def setUp(self):
        self.page = read("src/advprocessingoptions.cpp")

    def test_the_real_tooltip_is_kept_somewhere_it_can_be_restored_from(self):
        self.assertIn("cecAvailableTooltip_ = tr(", self.page)
        self.assertIn("cecCheckBox->setToolTip(cecAvailableTooltip_);", self.page)
        self.assertIn("QString cecAvailableTooltip_;",
                      read("src/advprocessingoptions.h"))

    def test_the_restore_does_not_read_what_it_is_about_to_write(self):
        block = body(self.page, "void AdvProcessingOptions::updateCecAvailability()")
        self.assertIn("? cecAvailableTooltip_", block)
        self.assertNotIn("? cecCheckBox->toolTip()", block)


class NoHexEscapeInTheNewText(unittest.TestCase):
    """Same trap as test_hex_escape_greed_static, checked on this page's
    strings because both new tooltips are full of en dashes."""

    def test_the_page_is_clean(self):
        backslash = chr(92)
        greedy = re.compile(re.escape(backslash) + r"x[0-9a-fA-F]{2}(?=[0-9a-fA-F])")
        self.assertEqual(greedy.findall(read("src/advprocessingoptions.cpp")), [])


if __name__ == "__main__":
    unittest.main()
