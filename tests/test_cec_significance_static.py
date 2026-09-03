"""The CEC significance test, and the estimate it cannot run without.

Zahn et al. (2022) apply no significance test, and on a well-coupled daytime
record none is needed. It is the weak-turbulence case this guards. The octants
mean something only if the sign of c' carries a surface signature; where the
scalar is barely coupled to the vertical wind that sign is noise, the moist
ejections split near evenly between O1 and O2, and the method returns an E/T and
a Reco/P split of a signal that is not there. Nothing else in CEC notices - the
occupancy gates see two well-filled octants and the components still sum to the
total.

So the dialog offers a threshold in multiples of the flux's own random error,
off by default. Finkelstein and Sims (2001) give that error from the period's
own integral timescale, and |F| / RE is about |r| * sqrt(N_indep / 2), which is
what makes the comparison a statement about the correlation rather than about an
assumed eddy size - so ticking the box switches random uncertainty estimation on,
the same coupling the CEC checkbox has with WPL, and a triangle stays lit while
the project is in the state where the test cannot run.

The load-bearing detail is again WHICH SIGNAL each of those hangs off. The
auto-enable is a side effect the user did not ask for, so it hangs off `clicked`.
The triangle and the value write hang off `toggled`, which also fires when the
code sets the box - which is what makes `restoreDefaults` write the 0.
"""

from pathlib import Path
import unittest

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_PARSER = ENGINE_ROOT / "src" / "src_common" / "write_processing_project_variables.f90"


def read(relative):
    return (GUI_ROOT / relative).read_text(encoding="utf-8", errors="replace")


def body(source, signature):
    """One function's body, so a match cannot come from elsewhere in the file."""
    start = source.index(signature)
    return source[start:source.index("\n}\n", start)]


class TheSettingRoundTrips(unittest.TestCase):

    def test_the_key_and_its_default(self):
        self.assertIn('INI_PROJECT_82   = QStringLiteral("cec_min_flux_sigma")',
                      read("src/ecinidefs.h"))
        #> 0, and 0 is off: an untouched project reproduces the paper, which
        #> applies no such test. The same default the engine carries.
        self.assertIn("qreal cec_min_flux_sigma = 0.0;", read("src/ecprojectstate.h"))

    def test_it_is_written_read_compared_and_reset(self):
        proj = read("src/ecproject.cpp")
        self.assertIn("project_ini.setValue(EcIni::INI_PROJECT_82, "
                      "QString::number(ec_project_state_.projectGeneral"
                      ".cec_min_flux_sigma, 'f', 1))", proj)
        self.assertIn("project_ini.value(EcIni::INI_PROJECT_82,", proj)
        #> Without the comparison, switching the test on would not mark the
        #> project modified against its saved self and could be closed away.
        self.assertIn("qFuzzyCompare(ec_project_state_.projectGeneral.cec_min_flux_sigma, "
                      "previousProject.ec_project_state_.projectGeneral.cec_min_flux_sigma)",
                      proj)
        self.assertIn("ec_project_state_.projectGeneral.cec_min_flux_sigma = "
                      "defaultEcProjectState.projectGeneral.cec_min_flux_sigma;", proj)

    def test_an_out_of_range_value_reads_as_off(self):
        #> Falling back to "no test" is the safe direction to be wrong in: a
        #> project written by some later version keeps computing the published
        #> method rather than silently dropping periods.
        proj = read("src/ecproject.cpp")
        start = proj.index("const auto cecMinFluxSigma\n")
        block = proj[start:proj.index("readCecPairs", start)]
        self.assertIn("cecMinFluxSigma >= 0.0 && cecMinFluxSigma <= 10.0", block)
        self.assertIn("defaultEcProjectState.projectGeneral.cec_min_flux_sigma", block)


class TheDialogWiresIt(unittest.TestCase):

    def setUp(self):
        self.dialog = read("src/cecsettingsdialog.cpp")

    def test_unticked_writes_zero_rather_than_a_flag(self):
        """Off is 0, so the project file says plainly that no test runs and the
        engine needs no second key to ask whether this one counts."""
        self.assertIn("setGeneralCecMinFluxSigma(\n"
                      "                            on ? minFluxSigmaSpin->value() : 0.0)",
                      self.dialog)

    def test_ticking_it_switches_the_estimator_on(self):
        fn = body(self.dialog, "void CecSettingsDialog::enableRandomUncertainty()")
        #> Finkelstein and Sims. 1 is what the engine decodes as that.
        self.assertIn("setRandomErrorMethod(1)", fn)
        #> And only from off - a user who chose Mann and Lenschow keeps it.
        self.assertIn("if (ecProject_->randErrorMethod() != 0) { return; }", fn)

    def test_the_auto_enable_hangs_off_clicked_not_toggled(self):
        """`refresh()` blocks the project's signals, not the widgets', so
        `setChecked` there still emits `toggled`. Anything with a side effect
        the user did not ask for must therefore hang off `clicked`."""
        start = self.dialog.index("connect(minFluxSigmaBox, &QCheckBox::clicked")
        block = self.dialog[start:self.dialog.index("});", start)]
        self.assertIn("enableRandomUncertainty()", block)

        start = self.dialog.index("connect(minFluxSigmaBox, &QCheckBox::toggled")
        block = self.dialog[start:self.dialog.index("});", start)]
        self.assertNotIn("enableRandomUncertainty()", block)

    def test_the_triangle_is_lit_only_in_the_state_worth_flagging(self):
        """A warning icon that is usually on is one nobody reads."""
        fn = body(self.dialog, "void CecSettingsDialog::updateMinFluxSigmaWarning()")
        self.assertIn("minFluxSigmaBox->isChecked()", fn)
        self.assertIn("ecProject_->randErrorMethod() == 0", fn)
        self.assertIn("&&", fn)
        #> Hidden until something asks for it, and asked for on load - a
        #> project saved in that state should open with it already lit.
        self.assertIn("minFluxSigmaWarningLabel->hide();", self.dialog)
        self.assertIn("updateMinFluxSigmaWarning();",
                      body(self.dialog, "void CecSettingsDialog::refresh()"))

    def test_refresh_cannot_write_back_what_it_is_reading(self):
        fn = body(self.dialog, "void CecSettingsDialog::refresh()")
        self.assertIn("QSignalBlocker minFluxSigmaBoxBlocker(minFluxSigmaBox)", fn)
        self.assertIn("QSignalBlocker minFluxSigmaSpinBlocker(minFluxSigmaSpin)", fn)

    def test_restoring_defaults_switches_it_off(self):
        fn = body(self.dialog, "void CecSettingsDialog::restoreDefaults()")
        #> Unblocked, unlike refresh, so the untick writes the 0.
        self.assertIn("minFluxSigmaBox->setChecked(false);", fn)
        self.assertNotIn("QSignalBlocker", fn)

    def test_the_tooltip_says_which_way_is_stricter_and_what_the_paper_says(self):
        start = self.dialog.index("const auto minFluxSigmaTooltip")
        tip = self.dialog[start:self.dialog.index("minFluxSigmaBox->setToolTip", start)]
        self.assertIn("Higher values reject more periods", tip)
        self.assertIn("The paper suggests no value at all", tip)
        #> And that it will switch something else on, before it does.
        self.assertIn("ticking this box switches it on", tip)
        #> Both halves of the row carry it: the checkbox is the label here.
        self.assertIn("minFluxSigmaBox->setToolTip(minFluxSigmaTooltip);", self.dialog)
        self.assertIn("minFluxSigmaSpin->setToolTip(minFluxSigmaTooltip);", self.dialog)


class TheOtherPageHearsAboutIt(unittest.TestCase):
    """ru_meth is no longer written only by the control that owns it."""

    def test_the_project_announces_the_change(self):
        header = read("src/ecproject.h")
        self.assertIn("void randomErrorMethodChanged();", header)
        fn = body(read("src/ecproject.cpp"), "void EcProject::setRandomErrorMethod(int n)")
        #> Only on a real change, or setting the same value from the page's own
        #> handler would bounce straight back into it.
        self.assertIn("const auto changed = ec_project_state_.randomError.ru_method != n;", fn)
        self.assertIn("if (changed) { emit randomErrorMethodChanged(); }", fn)

    def test_the_statistical_page_re_reads_it(self):
        stats = read("src/advstatisticaloptions.cpp")
        self.assertIn("connect(ecProject_, &EcProject::randomErrorMethodChanged,\n"
                      "            this, &AdvStatisticalOptions::syncRandomErrorMethod);",
                      stats)
        fn = body(stats, "void AdvStatisticalOptions::syncRandomErrorMethod()")
        #> Blocked, or the checkbox writes the value straight back through
        #> updateRandomErrorArea and replaces Finkelstein and Sims with
        #> whatever the combo happens to be showing.
        self.assertIn("QSignalBlocker checkBoxBlocker(randomErrorCheckBox)", fn)
        self.assertIn("QSignalBlocker comboBlocker(randomMethodCombo)", fn)
        self.assertIn("randomErrorCheckBox->setChecked(method != 0)", fn)
        #> By stored value, not `method - 1`. That arithmetic held only while
        #> the menu listed exactly the first two methods; Billesbach is
        #> ru_meth 4 on row 2, because 3 is Mahrt and the menu does not offer
        #> it. See test_random_method_mapping_static.py, which pins the whole
        #> mapping - this only has to know that the page still points the
        #> combo at whatever CEC wrote.
        self.assertIn("randomMethodCombo->findData(method)", fn)
        self.assertIn("randomMethodCombo->setCurrentIndex(row)", fn)
        #> And the estimator's own settings follow, wherever the switch came
        #> from - one helper, so the two paths cannot drift apart.
        self.assertIn("setRandomErrorControlsEnabled(method != 0)", fn)
        self.assertIn("setRandomErrorControlsEnabled(b);",
                      body(stats, "void AdvStatisticalOptions::updateRandomErrorArea(bool b)"))


@unittest.skipUnless(ENGINE_PARSER.exists(),
                     "eddyflow-engine not checked out beside this repository")
class ItAgreesWithTheEngine(unittest.TestCase):

    def test_the_engine_reads_the_same_key_with_the_same_default(self):
        engine = ENGINE_PARSER.read_text(encoding="utf-8", errors="replace")
        tags = (ENGINE_ROOT / "src" / "src_common" / "m_common_global_var.f90") \
            .read_text(encoding="utf-8", errors="replace")
        self.assertIn("'cec_min_flux_sigma'", tags)
        self.assertIn("EddyFlowProj%cec%min_flux_sigma = 0d0", engine)

    def test_the_auto_enable_writes_the_value_the_engine_calls_finkelstein_sims(self):
        """The dialog writes 1. If the engine ever renumbered its methods, the
        box would be switching on something else."""
        engine = ENGINE_PARSER.read_text(encoding="utf-8", errors="replace")
        start = engine.index("select case (nint(EPPrjNTags(24)%value))")
        block = engine[start:engine.index("end select", start)]
        #> The case label immediately above the name, read out of the engine.
        arm = block[:block.index("finkelstein_sims_01")].rstrip().splitlines()[-2]
        self.assertEqual(arm.strip(), "case(1)")
        self.assertIn("setRandomErrorMethod(1)",
                      body(read("src/cecsettingsdialog.cpp"),
                           "void CecSettingsDialog::enableRandomUncertainty()"))


if __name__ == "__main__":
    unittest.main()
