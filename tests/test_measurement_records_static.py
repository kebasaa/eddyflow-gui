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

    def test_selection_travels_as_a_raw_column_not_a_label(self):
        """The payload is the raw column, not the 1-based record index.

        The dropdown offers H2O columns the project has not activated, and an
        index into the record list cannot name one of those - there is no
        record to point at until the selection creates it. The column names
        both kinds. `moistureRef` still stores an index internally.
        """
        self.assertIn("editor->addItem(choice.second, choice.first)", self.src)
        self.assertIn("combo->currentData().toInt()", self.src)
        self.assertIn("page_->setMoistureColumnForGas(gasIdx, value.toInt())",
                      self.src)
        self.assertIn("const int gasIdx = gasRecordIndex(row);", self.src,
                      "the record index is taken before the redraw and "
                      "carried into the queued step")
        self.assertNotIn("setMoistureRefForGas", self.src,
                         "a record index cannot name a column that has no "
                         "record yet")

    def test_display_uses_the_resolved_reference(self):
        """A stored 0 means auto, so the table must show what it resolves to."""
        self.assertIn("MeasurementRecords::resolveMoistureRef", self.src)

    def test_the_combo_commits_when_the_user_picks(self):
        """A QComboBox editor with no `activated` connection commits only on
        focus-out.

        Choosing from the popup closed the popup and updated the combo, and
        nothing reached the model - so the H2O column the selection switches on
        stayed unticked until the user clicked away or minimised the window and
        came back. The value did save eventually, which is what made it look
        like a repaint that had been lost rather than a commit that had not
        happened.

        The variable, irga and anem delegates all connect `activated` to the
        same commitData/closeEditor pair; this is that pattern, as a lambda
        because the class is declared in the .cpp with no Q_OBJECT.
        """
        start = self.src.index("if (index.column() == BasicVariableSelectionModel::Moisture)")
        body = self.src[start: self.src.index("return editor;", start)]
        self.assertIn("QOverload<int>::of(&QComboBox::activated)", body,
                      "the choice has to reach the model when it is made")
        self.assertIn("emit self->commitData(editor);", body)
        self.assertIn("emit self->closeEditor(editor,", body,
                      "committing without closing leaves the editor over the "
                      "cell it just wrote")

    def test_the_label_lookup_follows_the_new_key(self):
        """setEditorData preselects by display text, so the label a gas
        resolves to has to match one of the items. Comparing the reference
        index against a column number would match nothing and silently leave
        the combo unpreselected."""
        start = self.src.index("QString BasicSettingsPage::moistureLabelForGas")
        body = self.src[start: self.src.index("\n}", start)]
        self.assertIn("gases.at(ref - 1).rawColumn", body)
        self.assertIn("choice.first == rawColumn", body)


class EveryDescribedWaterIsOffered(unittest.TestCase):
    """The dropdown lists every H2O the raw file description names, not only
    the ones already checked in the variable table.

    A site whose gas sits on an analyser with its own hygrometer could not say
    so: `moistureChoices` walked the gas *records*, so an H2O column that had
    not been switched on was not on offer, and the gas went on being corrected
    with another analyser's water. Choosing one now switches it on.

    `hasMoistureCandidates` had the same fault and a worse symptom: it gates
    `moistureAvailable`, so a project whose H2O columns were all inactive got
    no moisture dropdown at all - precisely the project this is for.
    """

    @classmethod
    def setUpClass(cls):
        cls.src = _read(BASIC_PAGE)

    def _body(self, signature):
        start = self.src.index(signature)
        return self.src[start: self.src.index("\n}", start)]

    def test_choices_come_from_the_candidates_as_well_as_the_records(self):
        body = self._body(
            "QVector<QPair<int, QString>> BasicSettingsPage::moistureChoices")
        self.assertIn("ecProject_->gasColumns()", body)
        self.assertIn("candidatesForRole(static_cast<int>(VariableTableRole::H2o))",
                      body)
        self.assertIn("isNoneCandidateColumn", body,
                      "the None placeholder must not become a selectable H2O")
        self.assertIn("seen.contains", body,
                      "a column that is both a record and a candidate must "
                      "appear once")

    def test_inactive_entries_are_marked(self):
        """Choosing one adds a measured gas to the project. A label costs
        nothing and makes that expected rather than surprising."""
        body = self._body(
            "QVector<QPair<int, QString>> BasicSettingsPage::moistureChoices")
        self.assertIn("not yet selected", body)

    def test_both_kinds_take_the_analyser_from_the_metadata(self):
        """An inactive column has no record to read an instrument id from, so
        asking the metadata for both is what keeps the two labels alike."""
        body = self._body(
            "QVector<QPair<int, QString>> BasicSettingsPage::moistureChoices")
        self.assertIn("canonicalInstrumentForColumn(rawColumn)", body)

    def test_the_combo_appears_when_only_candidates_exist(self):
        body = self._body("bool BasicSettingsPage::hasMoistureCandidates")
        self.assertIn("candidatesForRole(static_cast<int>(VariableTableRole::H2o))",
                      body)


class ChoosingAnInactiveWaterActivatesIt(unittest.TestCase):
    """And it goes through addGasRecord, so the new record is configured."""

    @classmethod
    def setUpClass(cls):
        cls.src = _read(BASIC_PAGE)

    def _body(self, signature):
        """Body with comment-only lines dropped.

        The prose here names addGasRecord while explaining why the limit is
        asked *before* it, so an ordering check against the raw text compares
        against the explanation rather than the call.
        """
        start = self.src.index(signature)
        body = self.src[start: self.src.index("\n}", start)]
        return "\n".join(ln for ln in body.splitlines()
                         if not ln.lstrip().startswith("//"))

    def test_activation_reuses_addGasRecord(self):
        """A GasRecord built here would arrive with every per-gas setting at
        the -1 sentinel, and the writer emits no key for those - which the
        engine reads as "not configured" and answers by declining the test.
        That is the defect that cost CO2 and H2O their absolute limits; it must
        not come back through a second door."""
        body = self._body("bool BasicSettingsPage::setMoistureColumnForGas")
        self.assertIn("addGasRecord(kH2oSlug, rawColumn)", body)
        self.assertNotIn("GasRecord rec;", body)

    def test_the_limit_is_checked_before_adding_and_reported(self):
        """addGasRecord declines silently at the limit, which would leave the
        dropdown showing a choice that did nothing."""
        body = self._body("bool BasicSettingsPage::setMoistureColumnForGas")
        self.assertIn("gasLimitBlockReason(rawColumn)", body)
        self.assertIn("WidgetUtils::warning", body)
        self.assertLess(body.index("gasLimitBlockReason"),
                        body.index("addGasRecord"),
                        "asked before adding, not after")

    def _moisture_branch(self):
        start = self.src.index(
            "if (index.column() == Moisture && moistureAvailable(row) && page_)")
        return self.src[start: self.src.index("return true;", start)]

    def test_it_reports_whether_records_changed(self):
        """Activating a column checks a different row, so the model has to
        redraw rather than repaint one cell - and the redraw is driven by what
        the page reports, not by a constant."""
        self.assertIn("bool BasicSettingsPage::setMoistureColumnForGas", self.src)
        body = self._moisture_branch()
        self.assertIn(
            "const bool recordsChanged =\n                page_->setMoistureColumnForGas("
            "gasIdx, value.toInt());",
            body)
        self.assertIn("if (model && recordsChanged) { model->refresh(); }", body,
                      "the refresh is guarded by that answer")
        self.assertIn("emit dataChanged(", body,
                      "an ordinary re-pointing still only repaints its cell")

    def test_the_redraw_is_queued_out_of_the_delegate_callback(self):
        """This branch runs inside the delegate's setModelData, which
        QAbstractItemView calls from commitData() with the combo editor still
        open and still in its editor map.

        Resetting there makes the view release its editors from inside the call
        committing one, and the pending relayout is dropped: the newly ticked
        row appeared only at the next external repaint - minimising the window
        and coming back. Deferring one turn lets the editor close first.
        """
        body = self._moisture_branch()
        self.assertIn("QTimer::singleShot(0, this,", body)
        self.assertNotIn("beginResetModel();", body,
                         "a reset here runs inside QAbstractItemView::"
                         "commitData and its repaint is lost")
        self.assertIn("QPointer<BasicVariableSelectionModel> model(this);", body,
                      "the model may be gone by the time the timer fires")
        self.assertIn("QPointer<BasicSettingsPage> page(page_);", body)

    def test_the_table_redraws_before_the_dialog(self):
        """One queued call rather than two, so the order is fixed and the
        dialog appears over a table that already tells the truth."""
        body = self._moisture_branch()
        self.assertEqual(1, body.count("QTimer::singleShot"),
                         "two queued calls would leave the order to chance")
        lam = body[body.index("QTimer::singleShot"):]
        self.assertLess(lam.index("model->refresh();"),
                        lam.index("page->warnOnCrossAnalyserMoisture"))


class CellRecordsNameTheirAnalyser(unittest.TestCase):
    """Which analyser a cell temperature or pressure belongs to is stated in
    the raw file description, and the record has to keep up with it.

    `addNonGasRecord` resolved the instrument once, when the record was
    created, and the answer was never revisited. A cell column selected before
    its metadata row named an instrument kept the empty answer for the life of
    the project - a cached value going stale, not a control the user was
    missing.

    That is not cosmetic. The engine matches cell records to gases by analyser:
    an untagged cell record is shared by every gas, a tagged one reaches only
    its own, and a gas left without one falls back to *ambient* pressure. On a
    site with two closed-path analysers this silently computed one of them
    against the other's cell conditions, or against the open air - which is
    what a CO2/H2O/COS project did, with the LI-7200's cell pressure tagged and
    the MIRO's cell temperature not.

    A dropdown was briefly added here to let the assignment be chosen by hand.
    It was the wrong answer: the metadata already states it, so the interface
    applies it.
    """

    @classmethod
    def setUpClass(cls):
        cls.src = _read(BASIC_PAGE)

    def _body(self, signature):
        start = self.src.index(signature)
        return self.src[start: self.src.index("\n}", start)]

    def test_the_assignment_is_resynced_from_the_metadata(self):
        self.assertIn("void BasicSettingsPage::syncNonGasRecordInstruments",
                      self.src,
                      "a record that caches its analyser has to be able to "
                      "pick up a later metadata assignment")

    def test_it_runs_where_the_records_are_resolved(self):
        """Beside seedGasRecordsFromMetadata and resolveMigratedGasRecords.

        That position is reached through updateMetadataRead, which
        MainWindow::upgradeProjectInPlace invokes synchronously before anything
        is written - so the records resolve whether or not the user opens this
        page. The comment there records what happened the last time something
        in this family ran only when they happened to visit it first.

        Not refreshVariableTables: that is a display concern, and it is also
        reached from a biomet combo where this has nothing to do.
        """
        body = self._body("void BasicSettingsPage::reloadSelectedItems_1")
        for call in ("seedGasRecordsFromMetadata();",
                     "resolveMigratedGasRecords();",
                     "syncNonGasRecordInstruments();"):
            self.assertIn(call, body)
        self.assertLess(
            body.index("resolveMigratedGasRecords();"),
            body.index("syncNonGasRecordInstruments();"),
            "the instrument sync belongs with the other resolution steps")

        refresh = self._body("void BasicSettingsPage::refreshVariableTables")
        self.assertNotIn("syncNonGasRecordInstruments();", refresh,
                         "refreshing a table must not be what decides which "
                         "analyser a cell measurement belongs to")

    def test_the_metadata_is_mirrored_unconditionally(self):
        """There is no competing answer to protect: the raw file description
        is the authority, so `none` and `other` are mirrored like any other
        value. Only "cannot tell" - no metadata, or a column past its end -
        leaves the stored value alone."""
        body = self._body("void BasicSettingsPage::syncNonGasRecordInstruments")
        self.assertIn("resolved.isEmpty() || resolved == rec.instrumentId", body)
        self.assertNotIn("MeasurementRecords::otherInstrument()", body,
                         "the `other` carve-out protected a hand-made choice, "
                         "and there is no longer one to protect")

    def test_no_dropdown_offers_the_choice(self):
        for symbol in ("cellInstrumentChoices",
                       "nonGasRecordInstrument",
                       "setNonGasRecordInstrument",
                       "cellInstrumentEditable"):
            self.assertNotIn(symbol, self.src,
                             "%s asks again for something the raw file "
                             "description already states" % symbol)

    def test_the_instrument_column_stays_the_candidate_source(self):
        """One combo in this table, and it is the moisture one."""
        body = self._body("void setModelData")
        self.assertIn("combo->currentData().toInt()", body)
        self.assertNotIn("combo->currentData().toString()", body)


class CrossAnalyserWaterIsAnnounced(unittest.TestCase):
    """A gas corrected with a hygrometer on another analyser is told so - in
    the register the situation deserves.

    Legitimate - it is what a site with one hygrometer and two analysers has -
    and a compromise. The engine honours it now: the water vapour flux term is
    taken at that hygrometer's own lag and the dilution uses that cell's
    humidity. Both used to be declined outright while the mean WPL terms used
    the borrowed water anyway, so the gas came out corrected by a water the rest
    of the code held it did not share, with nothing to show for it.

    The dialog belongs to the *action* and the triangle to the *state*. A
    dialog raised on project open answers no question the user asked: they made
    no choice, and there is nothing to respond to. But a pairing that is
    already the case must not therefore go unmarked, which is what the triangle
    in the variable table is for.
    """

    @classmethod
    def setUpClass(cls):
        cls.src = _read(BASIC_PAGE)

    def _body(self, signature):
        start = self.src.index(signature)
        return self.src[start: self.src.index("\n}", start)]

    def test_one_predicate_decides_it(self):
        """The dialog, the triangle and the tooltip all ask the same question,
        so they ask it in one place. Three copies would be three chances for
        the table to disagree with the message."""
        self.assertEqual(
            1,
            self.src.count(
                "QString BasicSettingsPage::crossAnalyserWaterInstrument"),
            "one definition")
        body = self._body(
            "QString BasicSettingsPage::crossAnalyserWaterInstrument")
        self.assertIn("MeasurementRecords::resolveMoistureRef", body,
                      "the pairing has to be the one the engine will use")
        self.assertIn("gas.slug == kH2oSlug", body,
                      "a hygrometer does not borrow from itself")
        self.assertIn("gas.instrumentId == water.instrumentId", body)
        self.assertIn("MeasurementRecords::isRealInstrument", body,
                      "`other` and `none` are not identities, so a pairing "
                      "involving one says nothing about analysers")

    def test_the_dialog_names_both_analysers(self):
        body = self._body("void BasicSettingsPage::warnOnCrossAnalyserMoisture")
        self.assertIn("WidgetUtils::information", body)
        self.assertIn("crossAnalyserWaterInstrument(gasRecordIndex)", body)
        self.assertIn(".arg(species, gas.instrumentId, waterInstrument)", body)

    def test_the_dialog_does_not_claim_the_analyser_has_no_hygrometer(self):
        """It fires on an explicit choice, which a user may well make while
        their own analyser does measure H2O. The old text asserted otherwise."""
        body = self._body("void BasicSettingsPage::warnOnCrossAnalyserMoisture")
        self.assertNotIn("which carries no H<sub>2</sub>O", body)

    def test_it_fires_on_the_choice_and_not_on_project_open(self):
        """Reached from the table's moisture edit, one turn of the event loop
        later - never from the resolution step, which runs on every metadata
        read and would greet the user on opening a configured project."""
        start = self.src.index(
            "if (index.column() == Moisture && moistureAvailable(row) && page_)")
        branch = self.src[start: self.src.index("return true;", start)]
        self.assertIn("page->warnOnCrossAnalyserMoisture(gasIdx);", branch)
        resolved = self._body("void BasicSettingsPage::reloadSelectedItems_1")
        self.assertNotIn("warnOnCrossAnalyserMoisture", resolved,
                         "the resolution step runs on every metadata read, so "
                         "a dialog raised from it greets the user on opening "
                         "an already-configured project")

    def test_the_setter_raises_no_dialog_itself(self):
        """It is called from the delegate's setModelData. A modal dialog there
        spins a nested event loop inside QAbstractItemView::commitData with the
        combo editor still open, and over a table not yet redrawn.

        The blocked-limit warning stays in the setter but is queued; the
        cross-analyser one moved out to the caller so it can be ordered after
        the repaint.
        """
        body = self._body("bool BasicSettingsPage::setMoistureColumnForGas")
        self.assertNotIn("warnOnCrossAnalyserMoisture", body,
                         "the caller raises it, after redrawing the table")
        self.assertIn("QTimer::singleShot", body,
                      "the blocked-limit warning has to be queued too")
        warn = body.index("WidgetUtils::warning")
        queued = body.index("QTimer::singleShot")
        self.assertLess(queued, warn,
                        "the warning must sit inside the queued lambda")

    def test_no_once_per_pairing_bookkeeping_survives(self):
        """setMoistureRefForGas already returns early when the reference is
        unchanged, so the hash guarded nothing once the open-time call went.
        Re-picking a cross-analyser water is a fresh deliberate act."""
        self.assertNotIn("lastMoistureWarnPair_", self.src)


class CrossAnalyserWaterIsMarkedInTheTable(unittest.TestCase):
    """The state, as opposed to the action: a small warning triangle beside a
    gas whose hygrometer sits on another analyser."""

    @classmethod
    def setUpClass(cls):
        cls.src = _read(BASIC_PAGE)

    #: The branch every check in this class is about.
    BRANCH = "if (role == Qt::DecorationRole)"

    def _branch_at(self):
        self.assertIn(self.BRANCH, self.src,
                      "the warning triangle is gone: nothing marks a gas "
                      "corrected with another analyser's water")
        return self.src.index(self.BRANCH)

    def test_the_mark_is_a_decoration_on_the_variable_column(self):
        self._branch_at()
        self.assertIn("index.column() == Variable && !crossAnalyserWater(row).isEmpty()",
                      self.src)

    def test_it_is_not_asked_of_the_moisture_column(self):
        """paintComboCell fills that cell and draws a combo box from
        DisplayRole alone, so a decoration there is discarded - the mark would
        be dead code that reads as working."""
        start = self._branch_at()
        body = self.src[start: self.src.index("switch (index.column())", start)]
        self.assertNotIn("Moisture", body)

    def test_the_decoration_branch_precedes_the_role_filter(self):
        """data() drops every role but display, edit and tooltip. Below that
        line the branch is unreachable, and an unreachable mark is worse than
        none: the check would pass and the triangle would never appear."""
        self.assertLess(
            self._branch_at(),
            self.src.index("if (role != Qt::DisplayRole && role != Qt::EditRole"),
        )

    def test_the_icon_is_built_once(self):
        """data() runs for every visible cell on every repaint, so scaling a
        pixmap inside it would do that work continuously."""
        self.assertIn("static QIcon crossAnalyserIcon()", self.src)
        self.assertIn("static QIcon icon = []", self.src)
        self.assertIn(":/icons/msg-warning", self.src)
        self.assertIn("QSize(12, 12)", self.src)

    def test_the_tooltip_explains_the_triangle(self):
        self.assertIn("a different analyser", self.src,
                      "hovering the gas has to say what the mark means")

    def test_the_mark_follows_the_shared_predicate(self):
        start = self.src.index("QString crossAnalyserWater(const VariableTableCandidate& row) const")
        body = self.src[start: self.src.index("\n    }", start)]
        self.assertIn("page_->crossAnalyserWaterInstrument(idx)", body)
        self.assertIn("isActive(row)", body,
                      "an unchecked row has no record and nothing to mark")


if __name__ == "__main__":
    unittest.main()
