"""Gas records follow their gas when the metadata changes.

A gas record states its species and analyser but finds its data by raw column
number, and nothing re-checked that number after the metadata changed. Add or
remove a variable in the metadata editor, switch metadata files, or re-declare
a column, and the record kept pointing at a column that now held something
else. Its row left the variable table, so the gas looked unticked; ticking it
again made a second record for the same measurement. Every per-gas table in
the advanced pages then listed the gas twice, and both records reached the
project file.

Two mechanisms fix it, and this file pins both:

- the metadata editor's insert and remove now reach BasicSettingsPage, which
  moves every column number in the project with the edit;
- every metadata read reconciles the gas records against the metadata: a
  stale record moves to its gas's column, a duplicate is merged into the older
  record, and a gas the metadata no longer declares is removed, with a message.

Removal goes through one EcProject helper, so the moisture, lag-donor and CEC
indices into the list are renumbered the same way compaction and the
primary-instrument reorder renumber them.
"""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]

PROJECT = "src/ecproject.cpp"
PROJECT_H = "src/ecproject.h"
PAGE = "src/basicsettingspage.cpp"
PAGE_H = "src/basicsettingspage.h"
MODEL = "src/variable_model.cpp"
MODEL_H = "src/variable_model.h"
DESC_TAB = "src/dlrawfiledesctab.cpp"
DL_H = "src/dlproject.h"
MAIN_WIDGET = "src/mainwidget.cpp"


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def code(path):
    """Source with comment-only lines dropped, so prose cannot satisfy a
    search meant for code."""
    out = []
    for ln in read(path).splitlines():
        s = ln.lstrip()
        if s.startswith("//") or s.startswith("/*") or s.startswith("*"):
            continue
        out.append(ln)
    return "\n".join(out)


def body(path, signature):
    """The braces of one function, by brace counting."""
    src = code(path)
    start = src.index(signature)
    depth = 0
    seen = False
    for i in range(start, len(src)):
        if src[i] == "{":
            depth += 1
            seen = True
        elif src[i] == "}":
            depth -= 1
            if seen and depth == 0:
                return src[start:i + 1]
    raise AssertionError("unterminated body for %s" % signature)


class ReconcileRunsOnEveryMetadataRead(unittest.TestCase):
    def setUp(self):
        self.reload = body(PAGE, "void BasicSettingsPage::reloadSelectedItems_1()")

    def test_it_is_called(self):
        self.assertIn("reconcileGasRecords();", self.reload)

    def test_after_the_species_are_resolved(self):
        """It matches on the species, which a migrated record only has once
        resolveMigratedGasRecords has filled it in."""
        self.assertLess(self.reload.index("resolveMigratedGasRecords();"),
                        self.reload.index("reconcileGasRecords();"))

    def test_before_the_tables_are_built(self):
        """A table built from a stale record draws the gas unticked, and the
        user ticks it a second time."""
        self.assertLess(self.reload.index("reconcileGasRecords();"),
                        self.reload.index("refreshVariableTables();"))


class ReconcileDecidesByTheMetadata(unittest.TestCase):
    def setUp(self):
        self.fn = body(PAGE, "void BasicSettingsPage::reconcileGasRecords()")

    def test_silence_is_not_a_contradiction(self):
        """No metadata, or a column past its end, says nothing about the
        record, the same rule the non-gas prune follows."""
        self.assertIn("if (!vars || vars->isEmpty()) { return; }", self.fn)
        self.assertIn("gas.rawColumn > vars->size()", self.fn)

    def test_a_stale_record_moves_rather_than_goes(self):
        self.assertIn("gas.rawColumn = best;", self.fn)

    def test_the_older_duplicate_is_kept(self):
        """The lower index keeps its settings and every index that points at
        it; the newer one, seeded with defaults, goes."""
        self.assertIn("if (i < other)", self.fn)
        self.assertIn("toRemove.append(other);", self.fn)

    def test_existing_exact_duplicates_are_merged(self):
        """The state a project is saved in once a stale gas was ticked
        twice."""
        self.assertIn("firstOf.contains(key)", self.fn)

    def test_removal_goes_through_the_one_helper(self):
        self.assertIn("removeGasRecordsAt(toRemove);", self.fn)
        self.assertNotIn("gases.remove(", self.fn)
        self.assertNotIn("gases.removeAt(", self.fn)

    def test_the_user_is_told(self):
        self.assertIn("noteGasRecordChanges(", self.fn)

    def test_the_project_is_marked_only_when_something_changed(self):
        """It runs on every metadata read; storing unconditionally would mark
        every opened project modified."""
        self.assertIn("if (changed) { ecProject_->setGasColumns(gases); }",
                      self.fn)


class TheNoticeIsQueued(unittest.TestCase):
    """Reached from updateMetadataRead, which runs with the project's signals
    blocked and, on a silent upgrade, before the project is shown."""

    def test_it_is_deferred_and_coalesced(self):
        fn = body(PAGE, "void BasicSettingsPage::noteGasRecordChanges(")
        self.assertIn("QTimer::singleShot(0, this", fn)
        self.assertIn("if (gasNoticeQueued_) { return; }", fn)

    def test_reconcile_raises_no_dialog_itself(self):
        fn = body(PAGE, "void BasicSettingsPage::reconcileGasRecords()")
        self.assertNotIn("WidgetUtils::information", fn)
        self.assertNotIn("WidgetUtils::warning", fn)


class EditorEditsMoveTheColumns(unittest.TestCase):
    def test_the_model_announces_both_edits(self):
        insert = body(MODEL, "bool VariableModel::insertColumns(")
        remove = body(MODEL, "bool VariableModel::removeColumns(")
        self.assertIn("emit variableInserted(column);", insert)
        self.assertIn("emit variableRemoved(column);", remove)
        self.assertLess(insert.index("endInsertColumns();"),
                        insert.index("emit variableInserted(column);"))
        self.assertLess(remove.index("endRemoveColumns();"),
                        remove.index("emit variableRemoved(column);"))

    def test_the_signals_reach_the_page(self):
        self.assertIn("void rawColumnInserted(int column);", read(DL_H))
        self.assertIn("void rawColumnRemoved(int column);", read(DL_H))
        tab = code(DESC_TAB)
        self.assertIn("&VariableModel::variableInserted", tab)
        self.assertIn("&VariableModel::variableRemoved", tab)
        main = code(MAIN_WIDGET)
        self.assertIn("&BasicSettingsPage::onRawColumnInserted", main)
        self.assertIn("&BasicSettingsPage::onRawColumnRemoved", main)

    def test_every_record_kind_moves(self):
        fn = body(PAGE, "void BasicSettingsPage::shiftRawColumns(")
        for getter in ("gasColumns()", "cellColumns()", "diagColumns()",
                       "agcColumns()"):
            self.assertIn("shift(ecProject_->%s)" % getter, fn, getter)

    def test_the_single_column_keys_move(self):
        fn = body(PAGE, "void BasicSettingsPage::shiftRawColumns(")
        for key in ("GeneralColTs", "GeneralColAirT", "GeneralColAirP",
                    "ScreenFlag1Col", "ScreenFlag10Col"):
            self.assertIn("set" + key, fn, key)

    def test_biomet_columns_do_not_move(self):
        """Ambient T and P from the biomet are stored from 1000 up and are not
        raw columns."""
        fn = body(PAGE, "void BasicSettingsPage::shiftRawColumns(")
        self.assertIn("col < 1000", fn)

    def test_a_removed_gas_goes_through_the_one_helper(self):
        fn = body(PAGE, "void BasicSettingsPage::shiftRawColumns(")
        self.assertIn("removeGasRecordsAt(gone);", fn)
        self.assertIn("removeNonGasRecord(rec.slug, first);", fn)
        self.assertLess(fn.index("removeGasRecordsAt(gone);"),
                        fn.index("shift(ecProject_->gasColumns())"),
                        "the removed column's records go before the rest "
                        "move down onto it")


class EveryRearrangerCarriesTheCecPairs(unittest.TestCase):
    """The CEC carbon, water and extra channels are 1-based gas indices.
    Before, no rearrangement renumbered them, so removing a gas ahead of a pair
    moved the partition onto its neighbours."""

    def test_the_helper_exists(self):
        self.assertIn("static void remapCecPairs(", code(PROJECT))

    def test_all_three_use_it(self):
        for sig in ("void EcProject::compactGasRecords()",
                    "bool EcProject::setPrimaryGasInstrument(",
                    "bool EcProject::removeGasRecords("):
            self.assertIn("remapCecPairs(", body(PROJECT, sig), sig)

    def test_removal_is_declared(self):
        self.assertIn("bool removeGasRecords(const QVector<int>& indices);",
                      read(PROJECT_H))


class TheBiometSentinelSurvivesARemap(unittest.TestCase):
    """-1 means "the biomet RH". The remap used to send every non-positive
    value to automatic, so choosing a primary instrument forgot it."""

    def test_only_positive_references_are_remapped(self):
        fn = body(PROJECT, "static void remapMoistureRefs(")
        self.assertIn("if (gas.moistureRef > 0)", fn)


if __name__ == "__main__":
    unittest.main()
