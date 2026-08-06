"""Static checks on the measurement-record contract between GUI and engine.

The GUI writes gas/cell/diagnostic records into the [Project] group of the
.eddyflow file; the engine reads them by exact tag name. Nothing at build time
connects the two - they are separate repositories - so a rename on either side
produces a project the engine parses without complaint and processes with the
wrong columns.

These checks read the key names out of both sources and compare them. They are
skipped, not failed, when the engine checkout is not beside this one.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_TAGS = ENGINE_ROOT / "src" / "src_common" / "m_common_global_var.f90"
EC_PROJECT = GUI_ROOT / "src" / "ecproject.cpp"
BASIC_PAGE = GUI_ROOT / "src" / "basicsettingspage.cpp"
RECORD_HDR = GUI_ROOT / "src" / "measurement_record.h"
RECORD_SRC = GUI_ROOT / "src" / "measurement_record.cpp"


def _read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def _gui_record_keys():
    """Key names the GUI writes, reconstructed from the QStringLiteral parts.

    The writer builds them as a prefix plus a suffix, e.g.
    QStringLiteral("gas_%1_").arg(i + 1) + QStringLiteral("var").
    """
    src = _read(EC_PROJECT)
    start = src.index("void EcProject::writeMeasurementRecords")
    end = src.index("bool EcProject::readMeasurementRecords")
    body = src[start:end]

    keys = set()
    for prefix in ("gas", "cell", "diag"):
        if 'QStringLiteral("%s_num")' % prefix in body or (
            'prefix + QStringLiteral("_num")' in body and prefix != "gas"
        ):
            keys.add("%s_num" % prefix)
    # suffixes written for gas records
    gas_block = body[body.index('QStringLiteral("gas_%1_")'):]
    gas_block = gas_block[: gas_block.index("const auto writePlain")]
    for suffix in re.findall(r'p \+ QStringLiteral\("(\w+)"\)', gas_block):
        keys.add("gas_1_%s" % suffix)
    # suffixes written for the plain records
    plain = body[body.index("const auto writePlain"):]
    for suffix in re.findall(r'p \+ QStringLiteral\("(\w+)"\)', plain):
        keys.add("cell_1_%s" % suffix)
        keys.add("diag_1_%s" % suffix)
    return keys


def _engine_record_keys():
    text = _read(ENGINE_TAGS)
    pat = r"\b((?:gas|cell|diag)_(?:num|1_[a-z]+))\b"
    return set(re.findall(pat, text))


class MeasurementRecordContract(unittest.TestCase):
    def setUp(self):
        for path in (EC_PROJECT, RECORD_HDR, RECORD_SRC):
            self.assertTrue(path.is_file(), "missing %s" % path)

    def test_gui_and_engine_agree_on_record_keys(self):
        if not ENGINE_TAGS.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)
        gui = _gui_record_keys()
        engine = _engine_record_keys()
        self.assertTrue(gui, "no record keys found in the GUI writer")
        missing = sorted(gui - engine)
        self.assertFalse(
            missing,
            "the GUI writes keys the engine does not read: %s" % missing,
        )
        unwritten = sorted(engine - gui)
        self.assertFalse(
            unwritten,
            "the engine reads keys the GUI never writes: %s" % unwritten,
        )

    def test_writer_clears_stale_keys_before_writing(self):
        """Shrinking a list must not leave orphaned keys behind.

        pf_sect_* and wdf_sect_* have this bug today: remove a sector and the
        old keys stay in the file for the reader to find again.
        """
        body = _read(EC_PROJECT)
        start = body.index("void EcProject::writeMeasurementRecords")
        end = body.index("bool EcProject::readMeasurementRecords")
        writer = body[start:end]
        self.assertIn("childKeys()", writer)
        self.assertIn("remove(key)", writer)
        self.assertLess(
            writer.index("remove(key)"),
            writer.index('setValue(QStringLiteral("gas_num")'),
            "stale keys must be cleared before the new ones are written",
        )

    def test_migration_keeps_only_the_gases_the_file_named(self):
        """A legacy slot with no column becomes no record at all.

        All four used to be appended whether the site had them or not, so that
        record i stayed the engine's slot firstGas+i-1. Nothing reads a species
        from a position now - the record carries its own `var` - and the empty
        record was never free: the engine reserves a slot for every record it
        counts, so a named-but-absent gas reached every output as a column of
        error codes and took the species label with it.
        """
        body = _read(EC_PROJECT)
        start = body.index("void EcProject::migrateLegacyColumnsToRecords")
        end = body.index("void EcProject::writeMeasurementRecords")
        mig = body[start:end]
        for slug in ("co2", "h2o", "ch4"):
            self.assertIn('addGas(QStringLiteral("%s")' % slug, mig)
        self.assertIn("addGas(QString(), g.col_gas4)", mig)
        self.assertIn("if (col <= 0) { return; }", mig.split("addPlain")[0],
                      "migration must skip a slot the legacy file left empty")

    def test_the_flat_thresholds_are_matched_by_species(self):
        """migrateLegacyGasSettings can no longer read them off a position.

        The flat keys are labelled co2/h2o/ch4/other and used to be applied to
        records 0..3 directly. With the padding gone, a project without CO2 has
        water at position zero - and a positional read would hand it CO2's
        spike limit and absolute limits.
        """
        body = _read(EC_PROJECT)
        start = body.index("void EcProject::migrateLegacyGasSettings")
        mig = body[start:]
        mig = mig[: mig.index("\n}\n")]
        self.assertIn("legacySlotOf", mig)
        self.assertNotIn("const int n = std::min<int>(gases.size(), 4);", mig,
                         "the four-position walk is what this replaces")

    def test_records_without_a_column_are_dropped_on_load(self):
        """And the moisture references that pointed past one move with them.

        Compaction has to happen after every per-gas section has been read,
        because those sections are keyed by the position the record held in the
        file - doing it any earlier would read gas_4_al_min onto record three.
        And before ecProjectChanged(), so no page ever sees a placeholder.
        """
        src = _read(EC_PROJECT)
        self.assertIn("void EcProject::compactGasRecords()", src)

        body = src[src.index("void EcProject::compactGasRecords()"):]
        body = body[: body.index("\n}\n")]
        self.assertIn("rawColumn <= 0", body)
        self.assertIn("moistureRef", body,
                      "a 1-based index into this list has to be renumbered")

        load = src[src.index("bool EcProject::loadEcProject("):]
        load = load[: load.index("\n}\n")]
        self.assertLess(load.index("compactGasRecords()"),
                        load.index("emit ecProjectChanged()"),
                        "compaction must precede the signal the pages act on")

    def test_removing_a_gas_renumbers_what_pointed_past_it(self):
        """validateReferences only clears; it cannot renumber.

        It has no way to tell that a still-in-range index now names a different
        gas, so the shift has to happen where the record is erased. This was
        already wrong for records past the fourth - the only ones that used to
        be erased - and dropping the pinning makes every record erasable.
        """
        src = _read(GUI_ROOT / "src" / "basicsettingspage.cpp")
        body = src[src.index("void BasicSettingsPage::removeGasRecord("):]
        body = body[: body.index("\n}\n")]
        self.assertIn("gases.remove(i)", body)
        self.assertNotIn("kLegacyGasSlots", body,
                         "no position is reserved any more")
        self.assertIn("--gas.moistureRef", body)

        cells = src[src.index("void BasicSettingsPage::removeNonGasRecord("):]
        cells = cells[: cells.index("\n}\n")]
        self.assertIn("--gas.cellRef", cells,
                      "cellRef indexes the cell list and shifts the same way")

    def test_the_variable_table_finds_its_record_by_species(self):
        """Not by a fixed row index.

        The four species rows each carried one, which worked only while the
        record list reserved a position for every one of them. A row that named
        its index would now edit whichever gas had moved into it - and that
        index drives the Moisture, Molecular weight and Diffusivity cells.
        """
        src = _read(GUI_ROOT / "src" / "basicsettingspage.cpp")
        self.assertIn("int BasicSettingsPage::gasRecordIndexFor(", src)
        self.assertNotIn("int recordIndex = -1;", src,
                         "the per-row fixed index is what this replaces")
        body = src[src.index("int gasRecordIndex(const VariableTableCandidate&"):]
        body = body[: body.index("\n    }")]
        self.assertIn("gasRecordIndexFor(", body)

    def test_auto_reference_is_zero(self):
        """0 must mean auto, in both the type and the resolver.

        An automatic reference re-resolves on every read, so a project whose
        H2O is deleted repairs itself. A sentinel of -1 or an explicit index
        would leave it pointing at nothing.
        """
        hdr = _read(RECORD_HDR)
        self.assertIn("int moistureRef = 0;", hdr)
        self.assertIn("int cellRef = 0;", hdr)
        src = _read(RECORD_SRC)
        self.assertIn("gas.moistureRef > 0", src)

    def test_resolver_excludes_other_and_none(self):
        """"Other" and "none" must not count as an instrument identity.

        Many unrelated variables carry "Other", so matching on it would pair a
        gas with an arbitrary H2O rather than its own analyser's.
        """
        hdr = _read(RECORD_HDR)
        self.assertIn("isRealInstrument", hdr)
        self.assertIn("noneInstrument()", hdr)
        self.assertIn("otherInstrument()", hdr)
        src = _read(RECORD_SRC)
        self.assertIn("isRealInstrument(gas.instrumentId)", src)


class MoistureColumn(unittest.TestCase):
    """The Moisture column's visibility rules, as specified.

    Blank and not editable on the H2O row itself, on inactive gases, and on
    anything that is not a gas; a dropdown on active non-H2O gases.
    """

    def setUp(self):
        self.src = _read(BASIC_PAGE)
        start = self.src.index("bool moistureAvailable(")
        self.avail = self.src[start:self.src.index("\n    }", start)]

    def test_column_exists_between_selection_and_molecular_weight(self):
        enum = self.src[self.src.index("        Active = 0,"):]
        enum = enum[: enum.index("ColumnCount")]
        order = [l.strip().rstrip(",") for l in enum.splitlines()
                 if l.strip() and not l.strip().startswith("//")]
        order = [o.split(" ")[0] for o in order]
        self.assertIn("Moisture", order)
        self.assertLess(order.index("Selection"), order.index("Moisture"))
        self.assertLess(order.index("Moisture"), order.index("MolecularWeight"))

    def test_blank_on_h2o_row(self):
        """Water is not corrected with itself, so it gets no moisture choice.

        This used to key off a dedicated GasH2o row kind. The row kinds are
        collapsed now - a gas row carries its species rather than being one
        of four fixed sorts - so the test asks the question the code now
        asks: is this row's species water?
        """
        self.assertIn('gasSlug(row) == QLatin1String("h2o")', self.avail)
        self.assertIn("return false", self.avail)

    def test_blank_on_non_gas_and_inactive_rows(self):
        self.assertIn("gasRecordIndex(row) < 0", self.avail)
        self.assertIn("!isActive(row)", self.avail)
        # The ambient table has no gases, so no moisture column at all.
        self.assertIn("!molecularColumns_", self.avail)

    def test_editable_only_when_available(self):
        self.assertIn(
            "if (index.column() == Moisture && moistureAvailable(row))",
            self.src,
            "the column must be editable exactly when it has a value")

    def test_selection_travels_as_record_index_not_label(self):
        """The engine matches on the record index; labels are display only."""
        self.assertIn("editor->addItem(choice.second, choice.first)", self.src)
        self.assertIn("combo->currentData().toInt()", self.src)

    def test_display_uses_the_resolved_reference(self):
        """A stored 0 means auto, so the table must show what it resolves to."""
        self.assertIn("MeasurementRecords::resolveMoistureRef", self.src)


if __name__ == "__main__":
    unittest.main()
