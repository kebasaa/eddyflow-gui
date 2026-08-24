"""The Burba correction needs an LI-7500 family analyser, and says so.

The engine decides this itself. `override_settings.f90:46-55` walks every gas
column and, if none names a model containing `li7500`, forces
`RPsetup%bu_corr = 'none'` — **silently**, with no warning number, unlike CEC
which at least emits `Warning(114)`. So a project that ticks this box without
such an analyser asks for a correction that is then dropped without a word.

The page therefore greys the box and clears the setting, so the interface and
the run agree on what is going to happen.

Three things here are load-bearing.

**The tooltip must be restored from the saved copy, never from the widget.**
This is the regression `TheUnavailableTooltipDoesNotStick` exists for on the CEC
side: restoring `checkBox->toolTip()` puts back whatever is there *now*, which
after one round of unavailability is the "unavailable" text — so the real
tooltip is lost for the rest of the session.

**Every path that can change the answer must end in `updateBurbaAvailability()`.**
`reset()` is the one that was missed: it does `setEnabled(true)` as the default
for a project that *can* have the correction, but whether this one can depends
on the metadata, which a reset does not touch. Without the trailing call it
handed back a clickable box.

**There is deliberately no SmartFlux veto**, unlike CEC. The engine clears
`bu_corr` in exactly three places — `configure_for_express.f90`,
`configure_for_md_retrieval.f90`, and the LI-7500 test itself — and none is the
embedded run environment. Asserted here so the divergence stays a decision
rather than becoming an oversight.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def body(source, signature):
    """The one function, not whatever follows it."""
    start = source.index(signature)
    return source[start:source.index("\n}\n", start)]


PAGE = read("src/advprocessingoptions.cpp")
PAGE_H = read("src/advprocessingoptions.h")
SPECTRAL = read("src/advspectraloptions.cpp")
IRGA = read("src/irga_desc.cpp")
IRGA_H = read("src/irga_desc.h")


class TheUnavailableTooltipDoesNotStick(unittest.TestCase):
    """Restoring from the widget puts back the text it had just overwritten."""

    def test_the_available_text_is_kept(self):
        self.assertIn("burbaAvailableTooltip_ = tr(", PAGE)
        self.assertIn("burbaCorrCheckBox->setToolTip(burbaAvailableTooltip_);", PAGE)
        self.assertIn("QString burbaAvailableTooltip_;", PAGE_H)

    def test_it_is_restored_from_the_copy_not_the_widget(self):
        chunk = body(PAGE, "void AdvProcessingOptions::updateBurbaAvailability()")
        self.assertIn("? burbaAvailableTooltip_", chunk)
        self.assertNotIn("? burbaCorrCheckBox->toolTip()", chunk)


class EveryPathThatCanChangeTheAnswerRecomputesIt(unittest.TestCase):

    #: Each caller, and the call that must be the last thing it does.
    CALLERS = ("void AdvProcessingOptions::reset()",
               "void AdvProcessingOptions::updateBurbaGroup(bool b)")

    def test_reset_recomputes_availability(self):
        """The one that was missed: setEnabled(true) is only the default."""
        chunk = body(PAGE, "void AdvProcessingOptions::reset()")
        self.assertIn("updateBurbaAvailability();", chunk)
        self.assertLess(chunk.index("burbaCorrCheckBox->setEnabled(true);"),
                        chunk.index("updateBurbaAvailability();"),
                        "recomputing before the default is set achieves nothing")

    def test_update_burba_group_recomputes_availability(self):
        chunk = body(PAGE, "void AdvProcessingOptions::updateBurbaGroup(bool b)")
        self.assertIn("updateBurbaAvailability();", chunk)

    def test_refresh_recomputes_availability(self):
        self.assertIn("updateBurbaAvailability();",
                      body(PAGE, "void AdvProcessingOptions::refresh()"))

    def test_metadata_changes_reach_it(self):
        """The input is DlProject, not the ecProject every other control uses."""
        self.assertIn("connect(dlProject_, &DlProject::projectChanged,", PAGE)
        self.assertIn("this, &AdvProcessingOptions::updateBurbaAvailability);", PAGE)


class TheSettingIsClearedWithTheBox(unittest.TestCase):
    """A greyed box over a project that still says 1 would be a lie."""

    def test_the_project_is_cleared_when_unavailable(self):
        chunk = body(PAGE, "void AdvProcessingOptions::updateBurbaAvailability()")
        self.assertIn("ecProject_->setScreenBuCorr(0);", chunk)

    def test_it_is_only_written_when_it_differs(self):
        """The setter marks the project modified unconditionally, and this runs
        on every metadata read - so an unguarded write dirties a project that
        was only opened."""
        chunk = body(PAGE, "void AdvProcessingOptions::updateBurbaAvailability()")
        self.assertIn("if (ecProject_->screenBuCorr() != 0)", chunk)

    def test_the_clear_does_not_re_enter_through_a_signal(self):
        chunk = body(PAGE, "void AdvProcessingOptions::updateBurbaAvailability()")
        self.assertIn("QSignalBlocker blocker(burbaCorrCheckBox);", chunk)


class ThereIsDeliberatelyNoSmartfluxVeto(unittest.TestCase):
    """CEC has one; this does not, and that is a decision."""

    def test_no_smartflux_test_in_the_availability_function(self):
        chunk = body(PAGE, "void AdvProcessingOptions::updateBurbaAvailability()")
        self.assertNotIn("smartfluxMode", chunk)

    def test_the_reason_is_recorded(self):
        i = PAGE.index("void AdvProcessingOptions::updateBurbaAvailability()")
        note = PAGE[max(0, i - 1400):i]
        self.assertIn("configure_for_express.f90", note)
        self.assertIn("override_settings.f90", note)

    def test_cec_still_has_one(self):
        """If CEC ever loses its veto the comparison above stops meaning
        anything, so it is pinned rather than assumed."""
        self.assertIn("smartfluxMode",
                      body(PAGE, "void AdvProcessingOptions::updateCecAvailability()"))


class TheModelSetMatchesTheEngine(unittest.TestCase):
    """The engine tests `index(model, 'li7500') /= 0` over every gas column -
    a SUBSTRING match, so any future li7500-something qualifies there
    automatically. This side is an exact list of four, so it does not. The two
    cannot be made identical (the engine matches ini tags, this matches display
    strings), so the list is pinned instead: if a fifth variant is added to
    DlProject::IRGA_MODEL_STRING_*, this test fails and someone has to decide.
    Failing is the point - silently greying a box the engine would have
    honoured is the outcome worth preventing."""

    def test_exactly_the_four_open_path_variants(self):
        chunk = body(IRGA, "bool IrgaDesc::isLi7500FamilyModel(const QString& model)")
        named = set(re.findall(r"getIRGA_MODEL_STRING_(\d+)\(\)", chunk))
        self.assertEqual({"2", "3", "12", "14"}, named,
                         "li7500, li7500a, li7500rs, li7500ds - and not li7200 "
                         "or li7700, which the engine's substring test excludes")

    def test_no_li7500_variant_has_appeared_without_being_considered(self):
        project = read("src/dlproject.cpp")
        tags = set(re.findall(r'IRGA_MODEL_STRING_(\d+) = QStringLiteral\("(li7500\w*)"\)',
                              project))
        self.assertEqual({("2", "li7500"), ("3", "li7500a"),
                          ("12", "li7500rs"), ("14", "li7500ds")}, tags,
                         "a new li7500 variant qualifies for Burba in the engine "
                         "by substring; add it to isLi7500FamilyModel too")


class BothPagesAskTheSameQuestion(unittest.TestCase):
    """They agreed on the model set and still kept their own copy of the walk."""

    def test_the_walk_lives_in_one_place(self):
        self.assertIn("static bool hasLi7500Family(const QList<IrgaDesc>* irgas);",
                      IRGA_H)

    def test_neither_page_keeps_its_own_loop(self):
        for source, name in ((PAGE, "AdvProcessingOptions"),
                             (SPECTRAL, "AdvSpectralOptions")):
            chunk = body(source, "bool %s::hasLi7500FamilyIrga() const" % name)
            self.assertIn("IrgaDesc::hasLi7500Family(", chunk)
            self.assertNotIn("for (const IrgaDesc&", chunk,
                             "%s still walks the list itself" % name)

    def test_the_shared_helper_tolerates_a_null_list(self):
        chunk = body(IRGA, "bool IrgaDesc::hasLi7500Family(const QList<IrgaDesc>* irgas)")
        self.assertIn("if (!irgas) { return false; }", chunk)


class NoHexEscapeInTheNewText(unittest.TestCase):
    """The tooltips are full of \\xe2\\x82\\x82-style escapes; a new one that
    picks up a stray hex escape renders as mojibake."""

    def test_no_hex_escapes(self):
        i = PAGE.index("burbaAvailableTooltip_ = tr(")
        chunk = PAGE[i:i + 2000]
        self.assertIsNone(
            re.search(r"\\x[0-9a-fA-F]{2}(?=[0-9a-fA-F])", chunk))


if __name__ == "__main__":
    unittest.main()
