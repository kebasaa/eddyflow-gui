"""The signal-strength screen, and saying when it cannot run.

Conditional Eddy Covariance can delete samples an analyser's own diagnostic
condemns, and the engine looks for that diagnostic by a column named exactly
`AGC` or `RSSI` **on the gas's own analyser**. Finding none it skips that
channel's screen and partitions anyway, which is right, and silent.

Two problems followed from that silence, and this file pins both fixes.

The first: the Raw File Description never offered AGC or RSSI among its
variable names, so there was no way to declare one from the interface at all.
The screen was unreachable. They are in the list now.

The second: the cutoff sat in the CEC dialog promising a screen that in many
projects never ran. On the site this was written for, no analyser declared a
diagnostic and the 70% setting did nothing whatever. The control is now greyed
when nothing can be screened, and flagged when only some channels can.

The spelling used to be load-bearing throughout: the engine compared
case-sensitively, so a column stored as `agc` was never found. That is now the
fallback path only - the project file carries `agc_<i>_*` records naming the
column and its analyser, and the engine reads those first and lower-cased. The
names still have to round-trip exactly, because a project written before the
records existed has nothing else, and because the fallback is what a
hand-written metadata file gets.

The record path itself, and the three engine faults that made the whole screen
unreachable, are pinned in the engine's
static_checks/test_signal_strength_records_static.py.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT.parent / "eddyflow-engine"


def read(relative, base=ROOT):
    return (base / relative).read_text(encoding="utf-8", errors="replace")


def body(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start)]


class TheRawFileDescriptionOffersThem(unittest.TestCase):
    def setUp(self):
        self.desc = read("src/variable_desc.cpp")

    def test_both_names_exist(self):
        self.assertIn('static const QString s(QStringLiteral("AGC"));', self.desc)
        self.assertIn('static const QString s(QStringLiteral("RSSI"));', self.desc)

    def test_they_are_not_translated(self):
        """They are stored verbatim and matched case-sensitively by the engine,
        so a translated display name would be written and never found."""
        for n in (35, 36):
            fn = body(self.desc, "VariableDesc::getVARIABLE_VAR_STRING_%d()" % n)
            self.assertIn("QStringLiteral", fn)
            self.assertNotIn("tr(", fn)

    def test_they_are_in_the_dropdown(self):
        listing = body(self.desc, "const QStringList VariableDesc::variableStringList()")
        for n in (35, 36):
            self.assertIn("getVARIABLE_VAR_STRING_%d()" % n, listing)

    def test_they_are_recognised_names_not_free_text(self):
        #> Everything the dropdown offers is excluded here; a name that is not
        #> becomes an editable custom variable, which is not what these are.
        custom = body(self.desc, "bool VariableDesc::isCustomVariable(const QString& var)")
        for n in (35, 36):
            self.assertIn("var != getVARIABLE_VAR_STRING_%d()" % n, custom)

    def test_they_carry_no_units(self):
        #> A bare percentage compared against a threshold, like the diagnostic
        #> words: dimensionless, no measure type, no conversion.
        diag = body(self.desc, "bool VariableDesc::isDiagnosticVar(const QString& var)")
        for n in (35, 36):
            self.assertIn("var == getVARIABLE_VAR_STRING_%d()" % n, diag)


class TheTokenIsWhatTheEngineReads(unittest.TestCase):
    def test_the_project_writes_the_exact_strings(self):
        proj = read("src/dlproject.cpp")
        self.assertIn('VARIABLE_VAR_STRING_35 = QStringLiteral("AGC");', proj)
        self.assertIn('VARIABLE_VAR_STRING_36 = QStringLiteral("RSSI");', proj)

    def test_they_round_trip_explicitly(self):
        """Both mapping functions end in `return s`, so these would round-trip
        on the fall-through alone. Stated anyway: the fall-through writes
        whatever the DISPLAY name happens to be, so renaming the display would
        silently change the token and turn the screen off."""
        proj = read("src/dlproject.cpp")
        to_ini = body(proj, "QString DlProject::toIniVariableVar(const QString& s)")
        from_ini = body(proj, "QString DlProject::fromIniVariableVar(const QString& s)")
        for n in (35, 36):
            self.assertIn("return DlProject::VARIABLE_VAR_STRING_%d;" % n, to_ini)
            self.assertIn("return VariableDesc::getVARIABLE_VAR_STRING_%d();" % n,
                          from_ini)

    @unittest.skipUnless((ENGINE / "src/src_common/gas_slot_resolution.f90").exists(),
                         "eddyflow-engine not checked out beside this repository")
    def test_the_engine_matches_the_same_two_strings(self):
        """The fallback path turns on these agreeing, case included.

        A change on either side - lowercasing here, a rename there - would
        leave the interface offering a column the engine never looks at on a
        project that predates the records, and nothing at runtime would say so.
        """
        engine = read("src/src_common/gas_slot_resolution.f90", ENGINE)
        self.assertIn("UserCol(j)%var /= 'AGC' .and. UserCol(j)%var /= 'RSSI'", engine)
        desc = read("src/variable_desc.cpp")
        self.assertIn('QStringLiteral("AGC")', desc)
        self.assertIn('QStringLiteral("RSSI")', desc)

    @unittest.skipUnless((ENGINE / "src/src_common/m_cec.f90").exists(),
                         "eddyflow-engine not checked out beside this repository")
    def test_the_engine_still_partitions_without_one(self):
        #> Which is what lets the interface say "only this screen is skipped"
        #> rather than "the partition will not run".
        cec = read("src/src_common/m_cec.f90", ENGINE)
        self.assertIn("if (sig_col <= 0 .or. sig_col > nuser) cycle", cec)


class TheCutoffSaysWhenItCannotRun(unittest.TestCase):
    def setUp(self):
        self.dialog = read("src/cecsettingsdialog.cpp")
        self.fn = body(self.dialog,
                       "void CecSettingsDialog::updateSignalStrengthAvailability()")

    def test_it_asks_the_raw_file_description(self):
        """Not the project and not the raw data - the metadata is where a
        column is declared, and it is the same table the engine reads."""
        self.assertIn("dlProject_->variables()", self.fn)

    def test_it_matches_the_engines_rule_including_the_instrument(self):
        for n in (35, 36):
            self.assertIn("VariableDesc::getVARIABLE_VAR_STRING_%d()" % n, self.fn)
        #> Per analyser, as the engine is: a diagnostic on one instrument does
        #> not screen a gas on another.
        self.assertIn("withDiagnostic.contains(gas.instrumentId)", self.fn)

    def test_the_instrument_is_normalised_before_it_is_compared(self):
        """VariableDesc::instrument() is the translated label the table shows -
        "Irga 1: LI-7200" - while the gas records store "li7200_1". Comparing
        the two matches nothing, and does it silently: every analyser would
        look unscreened and the triangle would never go out. DlProject exposes
        canonicalInstrumentId for exactly this, and says so in its comment."""
        self.assertIn("dlProject_->canonicalInstrumentId(var.instrument())", self.fn)
        self.assertNotIn("withDiagnostic.insert(var.instrument())", self.fn)

    def test_nothing_declared_greys_the_control(self):
        self.assertIn("signalStrengthSpin->setEnabled(anyAtAll);", self.fn)
        self.assertIn("signalStrengthLabel->setEnabled(anyAtAll);", self.fn)

    def test_a_partial_declaration_flags_but_does_not_grey(self):
        """The screen genuinely runs for the analysers that do declare one, so
        greying the box there would be a lie."""
        self.assertIn("!anyAtAll || !unscreened.isEmpty()", self.fn)
        self.assertIn("unscreened.join(", self.fn)

    def test_both_tooltips_say_the_partition_still_runs(self):
        #> The point of the whole message. A greyed control with no
        #> explanation reads as "CEC is broken".
        self.assertEqual(self.fn.count("partition"), 2,
                         "one of the two tooltips no longer says the partition "
                         "is unaffected")

    def test_it_is_recomputed_when_the_dialog_opens(self):
        #> The metadata changes on another page, and the dialog outlives it -
        #> advprocessingoptions keeps one instance and re-shows it.
        refresh = body(self.dialog, "void CecSettingsDialog::refresh()")
        self.assertIn("updateSignalStrengthAvailability();", refresh)
        self.assertIn("cecDialog_->refresh();", read("src/advprocessingoptions.cpp"))

    def test_the_dialog_is_given_the_metadata(self):
        self.assertIn("new CecSettingsDialog(this, ecProject_, dlProject_);",
                      read("src/advprocessingoptions.cpp"))


if __name__ == "__main__":
    unittest.main()
