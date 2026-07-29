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

    def test_migration_preserves_slot_order(self):
        """All four legacy gas slots become records, present or not.

        The engine puts record i in slot firstGas+i-1. Dropping an absent gas
        during migration would shift every later gas into the wrong slot and
        silently move its per-gas settings onto a different species.
        """
        body = _read(EC_PROJECT)
        start = body.index("void EcProject::migrateLegacyColumnsToRecords")
        end = body.index("void EcProject::writeMeasurementRecords")
        mig = body[start:end]
        for slug in ("co2", "h2o", "ch4"):
            self.assertIn('addGas(QStringLiteral("%s")' % slug, mig)
        self.assertIn("addGas(QString(), g.col_gas4)", mig)
        self.assertNotIn("if (col <= 0) { return; }", mig.split("addPlain")[0])

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
