"""Toggling SmartFlux mode must not recurse until the stack is gone.

Entering the mode with Ctrl+F from the Project Creation page killed the process
outright - no window, no dialog, exception 0xC00000FD (STATUS_STACK_OVERFLOW).
The backtrace showed one cycle repeating until the stack ran out:

    AdvOutputOptions::refresh
      -> updateSpectralAssessmentCreationAvailability
        -> EcProject::setSpectraFluxRunMode
          -> setModified(true) + emit updateInfo()
            -> AdvOutputOptions::refresh          (updateInfo is wired to it)

Two things made it a loop rather than a single pass. The setter announced a
change whether or not the value had changed, and in SmartFlux mode the
availability function calls it unconditionally on every pass. And refresh()
calls that function AFTER lifting its own signal guard, so the announcement was
never suppressed.

The setter is guarded now, which breaks this particular cycle. The re-entrancy
flag on refresh() is the structural half: forty other EcProject setters emit
updateInfo() unconditionally, and any of them reached from refresh's tail would
close the same loop.

The restore maps are here too because they are the other half of "enters and
leaves cleanly". They used to be positional vectors that were never cleared,
over lists whose length varies with the project's gas count.
"""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def body(source, signature):
    """One function's body, so a match cannot come from elsewhere in the file."""
    start = source.index(signature)
    return source[start:source.index("\n}\n", start)]


class TheRunModeSetterDoesNotAnnounceANonChange(unittest.TestCase):
    def test_it_returns_early_when_the_value_is_unchanged(self):
        fn = body(read("src/ecproject.cpp"),
                  "void EcProject::setSpectraFluxRunMode")
        self.assertIn("flux_run_mode == n", fn)
        #> The guard must come before the write and the emit, or it guards
        #> nothing.
        self.assertLess(fn.index("flux_run_mode == n"),
                        fn.index("setModified(true)"))
        self.assertLess(fn.index("flux_run_mode == n"),
                        fn.index("emit updateInfo()"))

    def test_the_availability_function_still_calls_it_unconditionally(self):
        """Not a bug - it is how a project saved with an assessment-only mode is
        brought back to the default. It is only safe because of the guard above,
        so if this ever stops being unconditional the guard's reason is gone."""
        fn = body(read("src/advoutputoptions.cpp"),
                  "void AdvOutputOptions::updateSpectralAssessmentCreationAvailability")
        self.assertIn("setSpectraFluxRunMode(0)", fn)


class RefreshCannotReenterItself(unittest.TestCase):
    def setUp(self):
        self.page = read("src/advoutputoptions.cpp")

    def test_the_guard_is_set_before_anything_else(self):
        fn = body(self.page, "void AdvOutputOptions::refresh")
        self.assertIn("if (refreshing_) { return; }", fn)
        self.assertIn("refreshing_ = true", fn)
        #> Cleared by a scope guard, not by hand: refresh has several early
        #> paths through its helpers and a missed reset would wedge the page.
        self.assertIn("qScopeGuard", fn)
        self.assertLess(fn.index("refreshing_ = true"),
                        fn.index("blockSignals(true)"))

    def test_the_flag_is_declared(self):
        self.assertIn("bool refreshing_", read("src/advoutputoptions.h"))


class TheRestoreStateIsKeyedByWidget(unittest.TestCase):
    """A positional std::vector that is never cleared restores the wrong
    widget's state on the second cycle, and runs off the end when the list
    grows - and the per-gas boxes make it grow."""

    def test_neither_page_keeps_a_positional_vector(self):
        for header in ("src/advoutputoptions.h", "src/basicsettingspage.h"):
            text = read(header)
            self.assertNotIn("std::vector<bool> oldEnabled", text, header)
            self.assertIn("QHash<QWidget*, bool> oldEnabled", text, header)

    def test_the_maps_are_cleared_on_the_way_out(self):
        for source, sig in (
                ("src/advoutputoptions.cpp", "void AdvOutputOptions::setSmartfluxUI"),
                ("src/basicsettingspage.cpp", "void BasicSettingsPage::setSmartfluxUI")):
            fn = body(read(source), sig)
            self.assertIn("oldEnabled.clear()", fn, source)
            #> Recorded once. A second entry would otherwise store the disabled
            #> state the first one imposed and hand that back as the original.
            self.assertIn("if (!oldEnabled.contains(w))", fn, source)
            #> A widget created since the mode was entered has nothing stored,
            #> and .at() used to abort on it.
            self.assertIn("oldEnabled.value(w, true)", fn, source)

    def test_the_visibility_map_stores_visibility(self):
        """It stored isEnabled() into the visibility vector, so a widget that
        happened to be disabled came back hidden. isHidden() rather than
        isVisible(): the Advanced page is usually not showing when the mode is
        entered, and isVisible() is false for every widget on a hidden page."""
        fn = body(read("src/advoutputoptions.cpp"),
                  "void AdvOutputOptions::setSmartfluxUI")
        self.assertIn("oldVisible.insert(w, !w->isHidden())", fn)
        self.assertIn("oldVisible.clear()", fn)


if __name__ == "__main__":
    unittest.main()
