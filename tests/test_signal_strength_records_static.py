"""Signal-strength records, and the stale records that aborted a run.

Two halves of one bug report: an LI-7200 with a diagnostic column and an AGC
column, and an engine that refused the project.

The AGC column was the innocent party. What killed the run was a `diag_72`
record still naming the column the user had since re-declared as AGC - written
when that column *was* the diagnostic, and never removed, because nothing here
prunes a record whose column has changed underneath it. The column then leaves
the variable table, so the row that would show the record is gone and the record
cannot be un-ticked: invisible in the interface, and fatal in the engine, which
found two records competing for its one diagnostic slot.

The other half is that declaring an AGC column achieved nothing anyway. It had
no representation in the project file at all - the engine inferred the analyser
and matched the name case-sensitively - so the conditional eddy covariance screen
those columns exist for could not be aimed. `agc_<i>_var/_instr/_col` says both,
and is derived from the raw file description rather than selected, so it cannot
go stale the way the diagnostic records could.
"""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def body(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}\n", start)]


class TheProjectCarriesTheSignalColumns(unittest.TestCase):
    """As records, beside the cell and diagnostic ones."""

    def setUp(self):
        self.project = read("src/ecproject.cpp")

    def test_the_state_holds_them(self):
        self.assertIn("QVector<MeasurementRecord> agcColumns;",
                      read("src/ecprojectstate.h"))
        header = read("src/ecproject.h")
        self.assertIn("agcColumns() const", header)
        self.assertIn("void setAgcColumns(const QVector<MeasurementRecord>& recs)",
                      header)

    def test_they_round_trip_through_the_shared_helpers(self):
        """The same writePlain/readPlain the cell and diagnostic records use,
        so the three blocks cannot drift into different shapes."""
        self.assertIn('writePlain(QStringLiteral("agc"), g.agcColumns);',
                      self.project)
        self.assertIn('readPlain(QStringLiteral("agc"), g.agcColumns);',
                      self.project)

    def test_an_empty_list_is_still_written(self):
        """`agc_num=0` and no agc_num at all say different things to the
        engine: zero means this site declares no signal strength, absent means
        the file predates the records and the engine should fall back to
        matching the variable name. The early return must not swallow a
        project whose only records are these."""
        fn = body(self.project,
                  "void EcProject::writeMeasurementRecords(QSettings& project_ini)")
        self.assertIn("&& g.diagColumns.isEmpty() && g.agcColumns.isEmpty()", fn)

    def test_a_changed_signal_column_invalidates_previous_results(self):
        """It screens samples out of the partition, so a run with a different
        one does not produce what the previous run produced."""
        fn = body(self.project,
                  "bool EcProject::fuzzyCompare(const EcProject& previousProject)")
        self.assertIn("projectGeneral.agcColumns", fn)


class TheyAreDerivedNotSelected(unittest.TestCase):
    """A column either is declared AGC or RSSI or it is not. There is nothing
    for the user to choose, so there is no row and no tick - and nothing that
    can be left behind by an edit."""

    def setUp(self):
        self.page = read("src/basicsettingspage.cpp")
        self.fn = body(self.page,
                       "void BasicSettingsPage::syncSignalStrengthRecords()")

    def test_it_runs_on_every_metadata_read(self):
        """Beside seedGasRecordsFromMetadata, in the reload that both branches
        of updateMetadataRead call - the alternative metadata file and the
        embedded one alike. updateMetadataRead is reached through a synchronous
        request before anything is written, so a record derived here is derived
        whether or not the user ever opens Basic Settings."""
        reload = body(self.page,
                      "void BasicSettingsPage::reloadSelectedItems_1()")
        self.assertIn("syncSignalStrengthRecords();", reload)
        self.assertIn("pruneStaleNonGasRecords();", reload)
        md = body(self.page, "void BasicSettingsPage::updateMetadataRead(")
        self.assertEqual(md.count("reloadSelectedItems_1();"), 2,
                         "one of the two metadata branches no longer reloads")

    def test_the_prune_runs_before_anything_reads_the_records(self):
        """Before the instruments are synced, so a record about to be dropped
        is not first given an analyser, and before the variable tables are
        built, so a row cannot be drawn from a record that is going."""
        reload = body(self.page,
                      "void BasicSettingsPage::reloadSelectedItems_1()")
        self.assertLess(reload.index("pruneStaleNonGasRecords();"),
                        reload.index("syncNonGasRecordInstruments();"))
        self.assertLess(reload.index("syncSignalStrengthRecords();"),
                        reload.index("refreshVariableTables();"))

    def test_it_rebuilds_wholesale(self):
        """Patching would leave a record for a column that has since become
        something else - which is the very failure the prune below exists to
        undo."""
        self.assertIn("QVector<MeasurementRecord> records;", self.fn)
        self.assertIn("ecProject_->setAgcColumns(records);", self.fn)

    def test_it_reads_the_raw_file_description(self):
        self.assertIn("dlProject_->variables()", self.fn)
        for n in (35, 36):
            self.assertIn("VariableDesc::getVARIABLE_VAR_STRING_%d()" % n,
                          self.fn)

    def test_ignored_and_non_numeric_columns_are_skipped(self):
        """The description saying a column holds nothing usable is the
        description saying it is not a signal strength."""
        self.assertIn('var.ignore() == QLatin1String("yes")', self.fn)
        self.assertIn('var.numeric() == QLatin1String("no")', self.fn)

    def test_the_analyser_is_normalised(self):
        """canonicalInstrumentForColumn, not VariableDesc::instrument(): the
        latter is the translated label the table shows, and the records store
        the id. Comparing the two matches nothing, silently."""
        self.assertIn("rec.instrumentId = canonicalInstrumentForColumn(k + 1);",
                      self.fn)

    def test_the_slug_is_lower_cased(self):
        """The engine compares the record lower-cased, which is what makes the
        record path immune to the spelling. Deriving the slug from the display
        name keeps the two in step through a rename."""
        self.assertIn("rec.slug = name.toLower();", self.fn)

    def test_it_only_stores_when_something_changed(self):
        """setAgcColumns marks the project modified, and this runs on every
        metadata read - storing unconditionally would make merely opening a
        project dirty it."""
        self.assertIn("if (records != ecProject_->agcColumns())", self.fn)
        self.assertIn("bool operator==(const MeasurementRecord& other) const",
                      read("src/measurement_record.h"))


class ARecordWhoseColumnChangedIsDropped(unittest.TestCase):
    """The half that made the engine refuse the project."""

    def setUp(self):
        self.page = read("src/basicsettingspage.cpp")

    def test_every_non_gas_slug_has_a_variable_to_match(self):
        """Any slug this cannot police is left alone rather than dropped, so
        the map must actually cover the ones the interface writes."""
        fn = body(self.page,
                  "QString BasicSettingsPage::variableForNonGasSlug(const QString& slug)")
        for slug, n in (("cell_t", 15), ("int_t_1", 9), ("int_t_2", 10),
                        ("int_p", 11), ("diag_75", 25), ("diag_72", 26),
                        ("diag_77", 27), ("diag_anem", 30)):
            self.assertIn('slug == QLatin1String("%s")' % slug, fn, slug)
            self.assertIn("getVARIABLE_VAR_STRING_%d()" % n, fn, slug)

    def test_the_slugs_are_the_ones_actually_written(self):
        """A slug in the map that nothing writes polices nothing."""
        fn = body(self.page, "QString BasicSettingsPage::nonGasSlugForRole(int role)")
        for slug in ("cell_t", "int_t_1", "int_t_2", "int_p",
                     "diag_75", "diag_72", "diag_77"):
            self.assertIn('QStringLiteral("%s")' % slug, fn, slug)
        #> The anemometer keeps a combo of its own rather than a table row.
        self.assertIn('rec.slug = QStringLiteral("diag_anem");', self.page)

    def test_a_contradiction_removes_the_record(self):
        fn = body(self.page, "void BasicSettingsPage::pruneStaleNonGasRecords()")
        self.assertIn("vars->at(rec.rawColumn - 1).variable() == expected", fn)
        self.assertIn("records.removeAt(i);", fn)

    def test_it_walks_backwards(self):
        """Removing while stepping forwards skips the element after each
        removal, so two adjacent stale records left the second in place."""
        fn = body(self.page, "void BasicSettingsPage::pruneStaleNonGasRecords()")
        self.assertIn("for (int i = records.size() - 1; i >= 0; --i)", fn)

    def test_silence_is_not_a_contradiction(self):
        """No metadata loaded, or a shorter file open for the moment, says
        nothing about the record. Only an actual disagreement removes it -
        otherwise opening a project before its metadata would empty it."""
        fn = body(self.page, "void BasicSettingsPage::pruneStaleNonGasRecords()")
        self.assertIn("if (expected.isEmpty()) { continue; }", fn)
        self.assertIn("rec.rawColumn > vars->size()", fn)

    def test_it_covers_both_record_kinds(self):
        fn = body(self.page, "void BasicSettingsPage::pruneStaleNonGasRecords()")
        self.assertIn("ecProject_->setCellColumns(cells);", fn)
        self.assertIn("ecProject_->setDiagColumns(diags);", fn)

    def test_gas_records_are_left_alone(self):
        """The same staleness is possible there, but a gas record carries a
        block of per-species processing settings and dropping one discards
        them silently. A separate decision from this one."""
        fn = body(self.page, "void BasicSettingsPage::pruneStaleNonGasRecords()")
        self.assertNotIn("gasColumns()", fn)


class SignalStrengthIsNotAGas(unittest.TestCase):
    """isCustomLabel means "a species this interface does not know", and an
    AGC column reaching it was offered as a custom fourth gas - a percentage
    selectable as something to compute a flux of."""

    def test_both_names_are_excluded(self):
        page = read("src/basicsettingspage.cpp")
        block = page[page.index("bool isCustomLabel ="):
                     page.index("bool isCustomLabel =") + 3000]
        block = block[:block.index(";")]
        for n in (35, 36):
            self.assertIn("varName != VariableDesc::getVARIABLE_VAR_STRING_%d()" % n,
                          block, str(n))


if __name__ == "__main__":
    unittest.main()
