"""Which analyser leads is record order, and record order is an index nobody
may reorder carelessly.

A site with two analysers has to say which one is the site's. The engine
already decided everything from record order - the full output emits its column
families in that order and numbers a repeated species by it, and
DesignatedGasSlot takes the first record of each species for the bare FLUXNET
names, the hygrometer behind the unsuffixed H/LE/ET, and the CEC pair - but
nothing let a user set that order, so it was whatever the Variables table
happened to produce.

So the interface reorders the records, and writes nothing else. No new project
key, no per-record flag, and no engine change: putting an analyser's records
first *is* making it primary.

That makes one thing dangerous. `moistureRef` is a 1-based index into the very
list being reordered. Move the records without rewriting it and a gas is
corrected against a different hygrometer than the one the user chose - the
failure `compactGasRecords` already carries a comment about, which it calls
invisible until a flux is wrong. There are now two rearrangers and one remap
helper, on purpose.

The rest is about the box telling the truth: it lists the analysers the records
actually name, and shows the one leading now rather than a stored answer that
could disagree with the file.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]

PROJECT = "src/ecproject.cpp"
PROJECT_H = "src/ecproject.h"
PAGE = "src/basicsettingspage.cpp"
PAGE_H = "src/basicsettingspage.h"
RECORD_H = "src/measurement_record.h"


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def code(path):
    """Source with comment-only lines dropped.

    Each of these files explains the retired behaviour in prose right where it
    used to stand, so a naive search matches the explanation and not the code.
    """
    out = []
    for ln in read(path).splitlines():
        s = ln.lstrip()
        if s.startswith("//") or s.startswith("/*") or s.startswith("*"):
            continue
        out.append(ln)
    return "\n".join(out)


def body(path, signature, opener="{", closer="}"):
    """The braces of one function, by brace counting."""
    src = code(path)
    start = src.index(signature)
    depth = 0
    seen = False
    for i in range(start, len(src)):
        if src[i] == opener:
            depth += 1
            seen = True
        elif src[i] == closer:
            depth -= 1
            if seen and depth == 0:
                return src[start:i + 1]
    raise AssertionError("unterminated body for %s" % signature)


class OneRemapForEveryRearrangement(unittest.TestCase):
    def test_the_helper_exists_and_is_shared(self):
        src = code(PROJECT)
        self.assertIn("static void remapMoistureRefs(", src)
        #> One definition and two call sites: compaction and the reorder.
        call_sites = src.count("remapMoistureRefs(") - 1
        self.assertEqual(
            2, call_sites,
            "both rearrangers must go through the one helper; found %d call "
            "sites" % call_sites)

    def test_compaction_uses_it(self):
        self.assertIn("remapMoistureRefs(kept, remap)",
                      body(PROJECT, "void EcProject::compactGasRecords()"))

    def test_the_reorder_uses_it(self):
        self.assertIn(
            "remapMoistureRefs(reordered, remap)",
            body(PROJECT, "bool EcProject::setPrimaryGasInstrument("))

    def test_neither_rewrites_moisture_refs_by_hand(self):
        """A second open-coded copy is how the two drift apart."""
        for sig in ("void EcProject::compactGasRecords()",
                    "bool EcProject::setPrimaryGasInstrument("):
            self.assertNotIn("gas.moistureRef =", body(PROJECT, sig),
                             "%s must go through remapMoistureRefs" % sig)


class TheMapIsBuiltBeforeAnythingMoves(unittest.TestCase):
    """Built from the old positions while they still hold. Derived afterwards
    it would describe the arrangement it was meant to translate out of."""

    def setUp(self):
        self.block = body(PROJECT, "bool EcProject::setPrimaryGasInstrument(")

    def test_the_remap_is_filled_while_reordering(self):
        self.assertIn("remap[i + 1] = reordered.size()", self.block)

    def test_it_is_applied_after(self):
        self.assertLess(self.block.index("remap[i + 1] = reordered.size()"),
                        self.block.index("remapMoistureRefs(reordered, remap)"))

    def test_the_list_is_replaced_only_at_the_end(self):
        self.assertLess(self.block.index("remapMoistureRefs(reordered, remap)"),
                        self.block.index("gases = reordered"))


class TheReorderIsStable(unittest.TestCase):
    """Two passes over the original list, primary then rest. A sort would be
    free to permute an analyser's own gases, which the user arranged."""

    def setUp(self):
        self.block = body(PROJECT, "bool EcProject::setPrimaryGasInstrument(")

    def test_it_partitions_rather_than_sorts(self):
        self.assertIn("take(true)", self.block)
        self.assertIn("take(false)", self.block)
        for banned in ("std::sort", "std::stable_sort", "std::partition"):
            self.assertNotIn(banned, self.block)

    def test_an_empty_instrument_moves_nothing(self):
        """It must match no record rather than match every record with no
        instrument, or "no choice" would drag the unassigned ones to the top."""
        self.assertIn("!instrumentId.isEmpty()", self.block)

    def test_it_reports_whether_anything_moved(self):
        self.assertIn("if (!moved) { return false; }", self.block)


class NothingElseIsPersisted(unittest.TestCase):
    """The order is the whole setting. A key beside it could disagree with it,
    and then two places would answer the same question differently."""

    def test_no_primary_key_is_written(self):
        src = code(PROJECT)
        for key in ("primary_instr", "primary_gas", "fluxnet_default"):
            self.assertNotIn('QStringLiteral("%s' % key, src,
                             "the primary is record order; a stored key would "
                             "be a second answer to the same question")

    def test_the_current_primary_is_derived(self):
        block = body(PROJECT, "QString EcProject::primaryGasInstrument() const")
        self.assertIn("gasColumns", block)
        self.assertIn("isRealInstrument", block)
        self.assertIn("return rec.instrumentId", block)


class TheBoxTellsTheTruth(unittest.TestCase):
    def setUp(self):
        self.block = body(
            PAGE, "void BasicSettingsPage::refreshPrimaryInstrumentCombo()")

    def test_it_lists_the_analysers_the_records_name(self):
        self.assertIn("ecProject_->gasColumns()", self.block)
        self.assertIn("isRealInstrument", self.block)

    def test_it_shows_the_one_leading_now(self):
        self.assertIn("ecProject_->primaryGasInstrument()", self.block)
        self.assertIn("findData(leading)", self.block)

    def test_there_is_no_automatic_entry(self):
        """Record order is the setting and nothing is stored beside it, so
        "nobody chose" and "this one is first" are the same file. An entry for
        the former could never be current after a reload and would do nothing
        when picked."""
        self.assertNotIn("Automatic", self.block)

    def test_it_is_disabled_when_there_is_no_choice(self):
        self.assertIn("setEnabled(instruments.size() > 1)", self.block)

    def test_rebuilding_does_not_look_like_a_user_choice(self):
        """The index is set programmatically here. On currentIndexChanged that
        would reorder the records behind the user every time the table
        refreshed."""
        self.assertIn("QSignalBlocker", self.block)
        self.assertIn(
            "QOverload<int>::of(&QComboBox::activated),\n"
            "            this, &BasicSettingsPage::onPrimaryInstrumentChanged",
            code(PAGE))


class ChoosingRebuildsWhatItChanged(unittest.TestCase):
    def setUp(self):
        self.block = body(
            PAGE, "void BasicSettingsPage::onPrimaryInstrumentChanged()")

    def test_it_reorders_through_the_project(self):
        self.assertIn("ecProject_->setPrimaryGasInstrument(", self.block)

    def test_it_refreshes_the_tables(self):
        """Half the rows just moved."""
        self.assertIn("refreshVariableTables()", self.block)

    def test_it_refreshes_itself(self):
        self.assertIn("refreshPrimaryInstrumentCombo()", self.block)

    def test_the_list_is_rebuilt_when_records_change(self):
        """An analyser appears or disappears with the gas selection, so the
        choices cannot be filled once at construction."""
        src = code(PAGE)
        self.assertIn("syncNonGasRecordInstruments();", src)
        self.assertLess(src.index("syncNonGasRecordInstruments();"),
                        src.index("refreshPrimaryInstrumentCombo();"))


class TheControlIsWhereTheUserLooks(unittest.TestCase):
    def test_it_sits_above_the_variables_table(self):
        src = code(PAGE)
        self.assertIn("varLayout->addLayout(primaryInstrumentLayout)", src)
        self.assertLess(
            src.index("varLayout->addLayout(primaryInstrumentLayout)"),
            src.index("varLayout->addWidget(fluxVariablesTable_)"))

    def test_it_is_not_hidden_with_the_retired_controls(self):
        """hiddenVariableControls parents a list of superseded widgets to the
        page and hides them. A new control added to that list is invisible."""
        src = code(PAGE)
        block = src[src.index("hiddenVariableControls"):]
        block = block[: block.index("};")]
        self.assertNotIn("primaryInstrumentCombo", block)

    def test_the_tooltip_says_it_changes_no_value(self):
        """The one thing a user needs to know before touching it."""
        text = read(PAGE)
        start = text.index("primaryInstrumentCombo->setToolTip(")
        tip = text[start: start + 1400]
        self.assertIn("no flux value", tip)
        self.assertIn("column_legend", tip)


if __name__ == "__main__":
    unittest.main()
