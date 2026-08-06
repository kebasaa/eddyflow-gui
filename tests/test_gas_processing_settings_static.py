"""Static checks on the per-gas processing settings written by the
Statistical Analysis page.

Advanced Settings used to hard-wire four gas slots. The page now generates one
row per gas record and stores the values on the record, under keys the engine
reads as per-gas overrides. As with the measurement records, nothing at build
time connects the two repositories, so a rename on either side yields a project
the engine parses happily and processes with the wrong thresholds.

The cross-repository check is skipped, not failed, when the engine checkout is
not beside this one.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_RP_TAGS = ENGINE_ROOT / "src" / "src_rp" / "m_rp_global_var.f90"
ENGINE_FCC_TAGS = ENGINE_ROOT / "src" / "src_fcc" / "m_fx_global_var_mod.f90"
ENGINE_READ_RP = ENGINE_ROOT / "src" / "src_rp" / "read_ini_rp.f90"
EC_PROJECT = GUI_ROOT / "src" / "ecproject.cpp"
STAT_PAGE = GUI_ROOT / "src" / "advstatisticaloptions.cpp"
STAT_HDR = GUI_ROOT / "src" / "advstatisticaloptions.h"
SPEC_PAGE = GUI_ROOT / "src" / "advspectraloptions.cpp"
SPEC_HDR = GUI_ROOT / "src" / "advspectraloptions.h"
OUT_PAGE = GUI_ROOT / "src" / "advoutputoptions.cpp"
OUT_HDR = GUI_ROOT / "src" / "advoutputoptions.h"
RECORD_HDR = GUI_ROOT / "src" / "measurement_record.h"
RECORD_SRC = GUI_ROOT / "src" / "measurement_record.cpp"

#: The settings the Statistical Analysis page owns, as record suffixes.
SUFFIXES = ("sr_lim", "al_min", "al_max", "ds_hf", "ds_sf", "tl_def")
#: The Spectral Corrections page, written in a FluxCorrection* section.
#:
#: `months` is the odd one out - a group list like `1-6,7-12` rather than a
#: number, and the only FCC per-gas TEXT tag. It replaced three flat tables of
#: twelve start/stop pairs each, labelled for CO2, CH4 and the fourth gas,
#: which is why every gas past the fourth used to inherit CO2's grouping.
SA_SUFFIXES = ("fmin", "fmax", "hfn_fmin", "min_st", "min_un", "max", "months")
#: The Output Files page, written in a RawProcess* section.
OUT_SUFFIXES = ("full_sp", "full_cosp_w", "raw")


def _read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def _writer_block(group):
    """The body of one write block, by its INI group constant."""
    src = _read(EC_PROJECT)
    start = src.index("project_ini.beginGroup(EcIni::%s)" % group)
    end = src.index("project_ini.endGroup()", start)
    return src[start:end]


def _screen_param_writer():
    """The body of the [RawProcess_ParameterSettings] write block."""
    return _writer_block("INIGROUP_SCREEN_PARAM")


class GasProcessingSettingsContract(unittest.TestCase):
    def setUp(self):
        for path in (EC_PROJECT, STAT_PAGE, STAT_HDR):
            self.assertTrue(path.is_file(), "missing %s" % path)

    def test_gui_and_engine_agree_on_setting_keys(self):
        if not ENGINE_RP_TAGS.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)
        writer = _screen_param_writer()
        written = set(re.findall(r'p \+ QStringLiteral\("(\w+)"\)', writer))
        self.assertEqual(
            written, set(SUFFIXES),
            "the page writes a different set of per-gas keys than expected")

        engine = _read(ENGINE_RP_TAGS)
        for suffix in sorted(written):
            self.assertIn(
                "'gas_1_%s'" % suffix, engine,
                "the GUI writes gas_N_%s but the engine has no such tag"
                % suffix)

    def test_written_only_where_the_record_holds_a_value(self):
        """A sentinel would override the legacy threshold with nonsense.

        The engine applies a per-gas override whenever the tag is *present*,
        not whenever it is meaningful.
        """
        writer = _screen_param_writer()
        for field in ("srLim", "alMin", "alMax", "dsHf", "dsSf", "tlDef"):
            self.assertIn("proc.%s >= 0.0" % field, writer)

    def test_stale_keys_are_cleared_before_writing(self):
        writer = _screen_param_writer()
        self.assertIn("childKeys()", writer)
        self.assertLess(
            writer.index("remove(key)"),
            writer.index('QStringLiteral("sr_lim")'),
            "stale keys must be cleared before the new ones are written")

    def test_stale_key_patterns_escape_the_digit_class(self):
        """`"\\d"` in a C++ literal is not a regex digit class.

        The compiler drops the unknown escape, leaving a pattern that matches
        `gas_d+_...` and therefore nothing at all - so shrinking the gas list
        would leave every orphaned key in the file.
        """
        src = _read(EC_PROJECT)
        patterns = re.findall(r'QStringLiteral\("\^gas_(\\+)d\+_', src)
        self.assertTrue(patterns, "no per-gas stale-key patterns found")
        for backslashes in patterns:
            self.assertEqual(
                len(backslashes), 2,
                "the digit class must be written \\\\d in a C++ literal")


class StatisticalPageRows(unittest.TestCase):
    """The four per-gas tables follow the records, not a fixed quartet."""

    def setUp(self):
        self.src = _read(STAT_PAGE)
        self.hdr = _read(STAT_HDR)

    def test_no_fixed_gas_widgets_remain(self):
        for name in ("despSpin_4", "despSpin_7", "absLimSpin_5",
                     "absLimSpin_12", "discontSpin_4", "discontSpin_15",
                     "timeLagSpin_3", "timeLagSpin_6"):
            self.assertNotIn(
                name + ";", self.hdr,
                "%s is a fixed gas slot and should be generated" % name)
        self.assertNotIn(
            "Defs::GAS4_STRING", self.src,
            "the fourth-gas label has no meaning once gases are records")

    def test_rows_are_built_from_the_gas_records(self):
        self.assertIn("rebuildGasRows", self.src)
        self.assertIn("ecProject_->gasColumns()", self.src)
        # The rows are ordered for reading rather than by record position, so
        # the skip that used to sit in the loop lives in the shared helper
        # now - see GasDisplayOrder below for the guard itself.
        self.assertIn("MeasurementRecords::gasDisplayOrder(gases)", self.src)


    def test_the_record_is_the_only_source(self):
        """The flat mirrors are retired: no writing, no reading, no fallback.

        Supersedes the pair of checks that pinned the mirror while it existed.
        A page that still mirrors is writing state nothing reads; one that
        still falls back is reading state nothing writes, which after the
        upgrade is always the built-in default and never the user's value.
        """
        self.assertNotIn(
            "if (gasIndex > 3) { return; }", self.src,
            "the mirror block should be gone")
        for setter in ("setScreenParamSrCo2Lim", "setScreenParamAlGas4Max",
                       "setScreenParamDsHfCh4", "setScreenParamTlDefH2o"):
            self.assertNotIn(setter, self.src,
                             "%s writes a retired flat key" % setter)

        body = self.src[self.src.index("double AdvStatisticalOptions::gasParamFor"):]
        body = body[: body.index("\n}")]
        for getter in ("screenParamSrCo2Lim()", "screenParamAlGas4Max()",
                       "screenParamTlDefH2o()"):
            self.assertNotIn(getter, body,
                             "%s is a retired flat fallback" % getter)
        self.assertIn("defaultGasParam(", body,
                      "the species default must be the only floor left")

    def test_other_pages_have_no_flat_fallback_either(self):
        for path, getters in [
            (SPEC_PAGE, ("spectraHfnCo2()", "spectraMaxGas4()", "spectraFminH2o()")),
            (OUT_PAGE, ("screenOutFullSpectraCo2()", "screenOutRawGas4()")),
            (GUI_ROOT / "src" / "timelagsettingsdialog.cpp",
             ("timelagOptCo2MinLag()", "timelagOptGas4MaxLag()")),
            (GUI_ROOT / "src" / "pwbtimelagsettingsdialog.cpp",
             ("pwbCo2MinLag()", "pwbGas4MaxLag()")),
        ]:
            src = _read(path)
            for getter in getters:
                self.assertNotIn(
                    getter, src,
                    "%s still falls back to %s" % (path.name, getter))


class GasDisplayOrder(unittest.TestCase):
    """The shared helper the three Advanced pages order their gases with.

    Record order is the engine's slot mapping and the key of every gas_<N>_*
    setting, so the pages sort a permutation of record *indices* and carry each
    index through as the widget's gasIndex. A helper that returned positions
    instead would silently point every widget at another gas.
    """

    def setUp(self):
        self.src = _read(RECORD_SRC)
        self.body = self.src[self.src.index("QVector<int> gasDisplayOrder("):]
        self.body = self.body[: self.body.index("\n}")]

    def test_records_with_no_column_are_left_out(self):
        # A record with no column is a slot kept for the engine's
        # record-to-slot mapping, not a measurement.
        self.assertIn("rawColumn > 0", self.body)

    def test_it_returns_record_indices(self):
        self.assertIn("order.append(i)", self.body)

    def test_the_sort_is_stable(self):
        # Two records that label the same keep record order between them.
        self.assertIn("std::stable_sort", self.body)
        self.assertIn("gasLabel(gases,", self.body)

    def test_every_page_that_shows_gases_uses_it(self):
        for path in (STAT_PAGE, SPEC_PAGE, OUT_PAGE):
            self.assertIn(
                "MeasurementRecords::gasDisplayOrder(gases)", _read(path),
                "%s builds its gas rows in record order" % path.name)


class BasicPageWritesTheAbsoluteLimitFloorToTheRecord(unittest.TestCase):
    """Choosing a species on the Basic Settings page seeds that gas's
    absolute-limit minimum from GasMetadata.

    It derived the floor by species and then wrote it to
    setScreenParamAlGas4Min() - the retired fourth-slot flat key. The engine
    reads al_gas4_min only as a legacy fallback and overrides it from
    gas_N_al_min immediately after (read_ini_rp.f90), so the value was computed
    correctly and discarded. The Statistical Analysis page has written this to
    the record since the settings were made per-gas; this is the same setting
    reached from the other page, and the two must agree.
    """

    @classmethod
    def setUpClass(cls):
        cls.src = _read(GUI_ROOT / "src" / "basicsettingspage.cpp")
        #: Comments stripped: the retired setter is named in a comment
        #: explaining why it is retired, which is worth keeping.
        cls.code = "\n".join(
            ln for ln in cls.src.splitlines()
            if not ln.lstrip().startswith("//"))

    def test_the_retired_flat_key_is_not_written(self):
        self.assertNotIn("setScreenParamAlGas4Min", self.code,
                         "the floor must land on the gas record, not on the "
                         "flat key the engine discards")

    def test_the_floor_lands_on_the_record(self):
        body = self.src[self.src.index(
            "void BasicSettingsPage::applyGasAbsoluteLimitMin"):]
        body = body[: body.index("\n}")]
        self.assertIn("proc.alMin", body)
        self.assertIn("setGasColumns", body,
                      "a mutated copy of the record list has to be put back")
        self.assertIn("defaultAbsoluteLimitMin", body)

    def test_the_seed_is_kept_per_record(self):
        """One string cannot say which record changed, so with more than one
        row whose species comes from the data the second edit reads as a
        repeat and is skipped."""
        hdr = _read(GUI_ROOT / "src" / "basicsettingspage.h")
        self.assertIn("QHash<int, QString> lastAbsLimitSpecies_", hdr)
        self.assertIn("applyGasAbsoluteLimitMin(int gasIndex", hdr,
                      "the slot must be told which record it is acting on")


class LegacySettingsMigration(unittest.TestCase):
    """Every per-gas setting must survive the upgrade from a 4.x project.

    The pages read a record's value and fall back to the legacy flat key when
    it still holds its sentinel. Retiring those flat keys is only safe if the
    migration has already carried them onto the records - otherwise an
    upgraded project silently reverts to the built-in defaults, with numbers
    that look plausible and are wrong.
    """

    def setUp(self):
        src = _read(EC_PROJECT)
        start = src.index("void EcProject::migrateLegacyGasSettings()")
        # The function ends at the first closing brace in column 0.
        end = src.index("\n}\n", start) + 3
        self.body = src[start:end]
        self.fields = set(re.findall(r'proc\.(\w+)', self.body))

    def test_every_processing_setting_is_migrated(self):
        """The migration must cover all of GasProcessingSettings."""
        hdr = _read(RECORD_HDR)
        struct = hdr[hdr.index("struct GasProcessingSettings"):]
        struct = struct[: struct.index("};")]
        declared = set(re.findall(r'(?:qreal|int)\s+(\w+)\s*=', struct))

        missing = sorted(declared - self.fields)
        self.assertFalse(
            missing,
            "these settings would be lost when the flat keys are retired: %s"
            % missing)

    def test_h2o_is_excluded_from_the_three_that_are_not_per_gas(self):
        """The water record's minimum flux and QA/QC thresholds are
        latent-heat ones.

        Migrating them onto the H2O record would move a latent-heat threshold
        onto a gas, which is the carve-out every converted page preserves.

        Excluded by species, not by index. `i == 1` is water only because
        migration lays the legacy slots down in that order - on a project that
        named only CO2 and CH4, index 1 is CH4 and it was CH4's minimum flux
        that got dropped.
        """
        self.assertIn('gases.at(i).slug == QLatin1String("h2o")', self.body)
        for field in ("toMinFlux", "saMinUn", "saMinSt", "saMax"):
            self.assertIn("proc.%s" % field, self.body)

    def test_migration_runs_after_the_whole_file_is_read(self):
        """The settings live in five sections read long after [Project].

        Calling it from migrateLegacyColumnsToRecords would read values that
        have not been loaded yet.
        """
        src = _read(EC_PROJECT)
        self.assertIn("migrateLegacyGasSettings();", src)
        call = src.index("migrateLegacyGasSettings();")
        columns = src.index("migrateLegacyColumnsToRecords();")
        self.assertGreater(
            call, columns,
            "the settings migration must run after the column migration")


class LegacyColumnRetirement(unittest.TestCase):
    """col_* is read for migration and never written."""

    def setUp(self):
        self.src = _read(EC_PROJECT)
        start = self.src.index("bool EcProject::saveEcProject")
        self.writer = self.src[start:self.src.index("bool EcProject::loadEcProject")]

    def test_the_retired_columns_are_not_written(self):
        for key in ("INI_PROJECT_18", "INI_PROJECT_19", "INI_PROJECT_20",
                    "INI_PROJECT_21", "INI_PROJECT_22", "INI_PROJECT_23",
                    "INI_PROJECT_24", "INI_PROJECT_27", "INI_PROJECT_28",
                    "INI_PROJECT_29", "INI_PROJECT_30", "INI_PROJECT_69",
                    "INI_PROJECT_31", "INI_PROJECT_32"):
            self.assertNotIn(
                "setValue(EcIni::%s," % key, self.writer,
                "%s is retired and must not be written" % key)

    def test_the_live_columns_are_still_written(self):
        """Ambient T/P and the sonic temperature are not per-instrument."""
        for key in ("INI_PROJECT_25", "INI_PROJECT_26", "INI_PROJECT_36"):
            self.assertIn("setValue(EcIni::%s," % key, self.writer)

    def test_the_retired_columns_are_still_read(self):
        """Migration needs them; a 4.x file is unreadable without them."""
        for key in ("INI_PROJECT_18", "INI_PROJECT_21", "INI_PROJECT_31"):
            self.assertIn("value(EcIni::%s" % key, self.src)

    def test_stale_keys_are_removed_from_upgraded_files(self):
        """QSettings keeps what it is not asked to overwrite.

        Without this an upgraded project carries col_co2 = 7 beside its
        records for ever - the orphan problem the record keys are cleared to
        avoid.
        """
        start = self.src.index("void EcProject::writeMeasurementRecords")
        body = self.src[start:self.src.index("bool EcProject::readMeasurementRecords")]
        self.assertIn("project_ini.remove(retired)", body)
        for key in ("INI_PROJECT_18", "INI_PROJECT_32"):
            self.assertIn(key, body)
        for live in ("INI_PROJECT_25", "INI_PROJECT_26", "INI_PROJECT_36"):
            self.assertNotIn(
                live, body, "%s is still a live key and must not be removed" % live)


class PreselectionSeedsRecords(unittest.TestCase):
    """Auto-selection fills records, not the retired columns.

    It used to write col_* while the table's checkboxes read records, so the
    preselection was invisible: a freshly parsed metadata file preselected
    gases into state the user could not see.
    """

    def setUp(self):
        self.src = _read(GUI_ROOT / "src" / "basicsettingspage.cpp")
        start = self.src.index("void BasicSettingsPage::seedGasRecordsFromMetadata")
        self.body = self.src[start:self.src.index("\n}\n", start)]

    def test_seeds_only_an_empty_record_list(self):
        self.assertIn("if (!ecProject_->gasColumns().isEmpty()) { return; }",
                      self.body,
                      "seeding must never overwrite an existing selection")

    def test_writes_records_not_columns(self):
        self.assertIn("setGasColumns(gases)", self.body)
        for setter in ("setGeneralColCo2", "setGeneralColH2o",
                       "setGeneralColCh4", "setGeneralColGas4"):
            self.assertNotIn(setter, self.body)

    def test_only_the_gases_the_site_measures_are_seeded(self):
        """A role the metadata has no column for produces no record.

        All four were appended regardless, so that record i stayed the engine's
        slot firstGas+i-1. Records name their own species now, and the empty
        one cost a column of error codes in every output file.
        """
        self.assertIn("gases.append(rec)", self.body)
        self.assertIn("if (rec.rawColumn <= 0) { continue; }", self.body)

    def test_measure_type_preference_is_explicit(self):
        """Ranked on the measure type, not on a translated combo label."""
        self.assertIn("getVARIABLE_MEASURE_TYPE_STRING_2", self.body)
        self.assertIn("getVARIABLE_MEASURE_TYPE_STRING_0", self.body)


class CellAndDiagnosticRecords(unittest.TestCase):
    """Cell T/P and diagnostics are records, not columns.

    They used to be written only as col_int_t_1 and friends. Retiring those
    keys without giving the table a record to write would lose the selection
    silently on save - it looks set in the interface and is absent from the
    file.
    """

    def setUp(self):
        self.src = _read(GUI_ROOT / "src" / "basicsettingspage.cpp")

    def test_every_non_gas_role_maps_to_a_slug(self):
        start = self.src.index("QString BasicSettingsPage::nonGasSlugForRole")
        body = self.src[start:self.src.index("\n}\n", start)]
        for slug in ("cell_t", "int_t_1", "int_t_2", "int_p",
                     "diag_75", "diag_72", "diag_77"):
            self.assertIn('"%s"' % slug, body)

    def test_the_table_writes_and_reads_those_records(self):
        self.assertIn("page_->addNonGasRecord(nonGasSlug, row.rawColumn)", self.src)
        self.assertIn("page_->removeNonGasRecord(nonGasSlug, row.rawColumn)", self.src)
        self.assertIn("page_->nonGasRecordExists(nonGasSlug, row.rawColumn)", self.src)

    def test_diagnostics_and_cells_go_to_different_lists(self):
        """A diagnostic in cellColumns would be read as a cell measurement."""
        self.assertIn("isDiagnosticSlug", self.src)
        self.assertIn("setDiagColumns(records)", self.src)
        self.assertIn("setCellColumns(records)", self.src)


class HiddenCombosNoLongerWriteState(unittest.TestCase):
    """The flux-table combos are candidate lists, not shadow state.

    They used to hold a second copy of the selection and push it into col_*
    on every change. That copy is retired, so the write path is gone: the
    combos survive only as the candidate lists rebuildVisibleRows() reads.
    """

    def setUp(self):
        self.src = _read(GUI_ROOT / "src" / "basicsettingspage.cpp")
        self.hdr = _read(GUI_ROOT / "src" / "basicsettingspage.h")

    def test_the_update_slots_are_gone(self):
        for slot in ("updateCo2RefCombo", "updateH2oRefCombo",
                     "updateCh4RefCombo", "updateFourthGasRefCombo",
                     "updateIntTcRefCombo", "updateIntT1RefCombo",
                     "updateIntT2RefCombo", "updateIntPRefCombo",
                     "updateDiag7500Combo", "updateDiag7200Combo",
                     "updateDiag7700Combo"):
            self.assertNotIn(slot, self.src, "%s writes a retired key" % slot)
            self.assertNotIn(slot, self.hdr)

    def test_reload_no_longer_restores_the_retired_columns(self):
        """reloadSelectedItems_1 used to read col_* back and rewrite it."""
        start = self.src.index("void BasicSettingsPage::reloadSelectedItems_1")
        body = self.src[start:self.src.index("\n}\n", start)]
        for getter in ("generalColCo2()", "generalColCh4()", "generalColIntTc()",
                       "generalColIntP()", "generalColDiag75()",
                       "generalColDiagAnem()"):
            self.assertNotIn(getter, body,
                             "%s is a retired key" % getter)

    def test_the_anemometer_diagnostic_stores_a_record(self):
        """It has a visible combo rather than a table row, so it is the one
        diagnostic that would otherwise have been left writing col_diag_anem.
        """
        start = self.src.index("void BasicSettingsPage::updateAnemFlagCombo")
        body = self.src[start:self.src.index("\n}\n", start)]
        self.assertNotIn("setGeneralColDiagAnem", body)
        self.assertIn('QStringLiteral("diag_anem")', body)
        self.assertIn("setDiagColumns(records)", body)
        self.assertIn("restoreAnemFlagFromRecord", self.src)


class CandidateListsReplaceTheCombos(unittest.TestCase):
    """The eleven flux-table combos are gone.

    They were QComboBoxes that were never laid out - widgets used as
    containers, which is what let them shadow the selection into col_*.
    """

    def setUp(self):
        self.src = _read(GUI_ROOT / "src" / "basicsettingspage.cpp")
        self.hdr = _read(GUI_ROOT / "src" / "basicsettingspage.h")

    def test_the_widgets_are_deleted(self):
        for combo in ("co2RefCombo", "h2oRefCombo", "ch4RefCombo",
                      "fourthGasRefCombo", "intTcRefCombo", "intT1RefCombo",
                      "intT2RefCombo", "intPRefCombo", "diag7500Combo",
                      "diag7200Combo", "diag7700Combo"):
            self.assertNotIn(combo, self.src, "%s should be gone" % combo)
            self.assertNotIn(combo, self.hdr)

    def test_the_ambient_combos_survive(self):
        """col_air_t, col_air_p and the biomet columns are live keys."""
        for combo in ("airTRefCombo", "airPRefCombo", "rhCombo", "rgCombo",
                      "lwinCombo", "ppfdCombo", "anemFlagCombo", "tsRefCombo"):
            self.assertIn(combo, self.hdr, "%s is still needed" % combo)


class InstrumentPruningRules(unittest.TestCase):
    """filterVariables encodes what each instrument can physically report.

    An open-path analyser measures a molar density and cannot report a mole
    fraction or a mixing ratio; a cell temperature or pressure exists only on
    a closed-path instrument. Offering those combinations would let a user
    build a project the engine cannot compute, so these are physics rules and
    not presentation.
    """

    def setUp(self):
        src = _read(GUI_ROOT / "src" / "basicsettingspage.cpp")
        start = src.index("void BasicSettingsPage::filterVariables()")
        self.body = src[start:src.index("\n}\n", start)]
        self.src = src

    def test_open_path_gases_lose_fraction_and_ratio(self):
        self.assertIn("isOpenPath(t) && isNotDensity(t)", self.body)
        for role in ("Co2", "H2o"):
            self.assertIn("pruneCandidates(VariableTableRole::%s" % role, self.body)

    def test_cell_measurements_are_pruned_for_open_path(self):
        for role in ("IntTc", "IntT1", "IntT2", "IntP"):
            self.assertIn("pruneCandidates(VariableTableRole::%s" % role, self.body)

    def test_each_diagnostic_keeps_only_its_own_analyser(self):
        for role in ("Diag7500", "Diag7200", "Diag7700"):
            self.assertIn("pruneCandidates(VariableTableRole::%s" % role, self.body)

    def test_pruning_walks_backwards(self):
        """Removing while stepping forwards skips the entry after each removal.

        Every one of these loops had that bug before conversion: two adjacent
        filterable candidates left the second one in the list.
        """
        start = self.src.index("void BasicSettingsPage::pruneCandidates")
        prune = self.src[start:self.src.index("\n}\n", start)]
        self.assertIn("for (int i = items.size() - 1; i >= 0; --i)", prune)
        self.assertIn("for (int i = anemFlagCombo->count() - 1; i >= 0; --i)",
                      self.body)

    def test_none_placeholders_are_never_pruned(self):
        start = self.src.index("void BasicSettingsPage::pruneCandidates")
        prune = self.src[start:self.src.index("\n}\n", start)]
        self.assertIn("contains(noneStr)) { continue; }", prune)


class FileFormatVersionCoupling(unittest.TestCase):
    """The GUI stamps a format version; the engine refuses anything newer.

    Bumping one without the other makes the engine reject every file the GUI
    writes, with a fatal error rather than a wrong number - the guard working
    as designed, but a complete break until both move.
    """

    def test_the_engine_supports_what_the_gui_writes(self):
        engine_var = ENGINE_ROOT / "src" / "src_common" / "m_common_global_var.f90"
        if not engine_var.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)

        gui = re.search(r'PROJECT_FILE_VERSION_STR\s*=\s*QStringLiteral\("([^"]+)"\)',
                        _read(GUI_ROOT / "src" / "defs.h"))
        engine = re.search(r"MaxSupportedIniVer\s*=\s*'([^']+)'", _read(engine_var))
        self.assertTrue(gui and engine, "version constants not found")

        def parts(v):
            return tuple(int(x) for x in v.group(1).split("."))

        self.assertLessEqual(
            parts(gui), parts(engine),
            "the GUI writes ini_version %s but the engine supports only %s - "
            "every project the GUI saves would be refused"
            % (gui.group(1), engine.group(1)))

    def test_a_project_without_records_is_refused_loudly(self):
        """Records are mandatory from 5.0.0; silence would mean no gas fluxes."""
        e2 = ENGINE_ROOT / "src" / "src_common" / "define_e2_set.f90"
        if not e2.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)
        src = _read(e2)
        self.assertNotIn("if (EddyFlowProj%gas_num <= 0) return", src,
                         "the silent early-out must be gone")
        self.assertIn("call ExceptionHandler(99)", src)


class SpectralSettingsContract(unittest.TestCase):
    """The Spectral Corrections page's six per-gas settings."""

    def test_written_in_a_fluxcorrection_section(self):
        """ReadIniFCC only sweeps sections whose name starts FluxCorrection.

        The FCC counterpart of the RawProcess* rule that caught the PWB
        conversion: put these beside gas_N_col in [Project] and the engine
        silently never sees them.
        """
        writer = _writer_block("INIGROUP_SPEC_SETTINGS")
        written = set(re.findall(r'p \+ QStringLiteral\("(\w+)"\)', writer))
        self.assertEqual(written, set(SA_SUFFIXES))
        defs = _read(GUI_ROOT / "src" / "ecinidefs.h")
        group = re.search(
            r'INIGROUP_SPEC_SETTINGS\s*=\s*QStringLiteral\("([^"]+)"\)', defs)
        self.assertTrue(group, "INIGROUP_SPEC_SETTINGS not found")
        self.assertTrue(
            group.group(1).startswith("FluxCorrection"),
            "the per-gas sa_* keys must live in a FluxCorrection* section")

    def test_gui_and_engine_agree_on_spectral_keys(self):
        if not ENGINE_FCC_TAGS.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)
        engine = _read(ENGINE_FCC_TAGS)
        for suffix in SA_SUFFIXES:
            self.assertIn(
                "'gas_1_sa_%s'" % suffix, engine,
                "the GUI writes gas_N_sa_%s but the engine has no such tag"
                % suffix)

    def test_h2o_has_no_qaqc_spins_of_its_own(self):
        """Water's thresholds are the latent-heat triple, not a gas flux.

        The same carve-out as leMinFluxSpin in the time-lag dialog. Giving an
        H2O record its own minimum/maximum would offer a setting with no
        counterpart in the engine.
        """
        src = _read(SPEC_PAGE)
        self.assertIn('gases.at(i).slug != QLatin1String("h2o")', src)

    def test_no_fixed_spectral_gas_widgets_remain(self):
        hdr = _read(SPEC_HDR)
        for name in ("spin14", "spin24", "spin34", "qcMinUnstableCo2Spin",
                     "qcMinStableGas4Spin", "qcMaxCh4Spin"):
            self.assertNotIn(name + ";", hdr, "%s is a fixed gas slot" % name)
        self.assertNotIn(
            "selectedColumnIsGas4", _read(SPEC_PAGE),
            "the fourth gas must not be defined negatively any more")


class OutputSettingsContract(unittest.TestCase):
    """The Output Files page's three per-gas flags."""

    def test_written_in_a_rawprocess_section(self):
        writer = _writer_block("INIGROUP_SCREEN_SETTINGS")
        written = set(re.findall(r'p \+ QStringLiteral\("(\w+)"\)', writer))
        self.assertEqual(written, set(OUT_SUFFIXES))
        defs = _read(GUI_ROOT / "src" / "ecinidefs.h")
        group = re.search(
            r'INIGROUP_SCREEN_SETTINGS\s*=\s*QStringLiteral\("([^"]+)"\)', defs)
        self.assertTrue(group.group(1).startswith("RawProcess"))

    def test_gui_and_engine_agree_on_output_keys(self):
        if not ENGINE_RP_TAGS.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)
        engine = _read(ENGINE_RP_TAGS)
        for suffix in OUT_SUFFIXES:
            self.assertIn("'gas_1_out_%s'" % suffix, engine)

    def test_engine_actually_reads_the_output_flags(self):
        """The tags existed for a long time with nothing reading them.

        rpGasOriginC was declared and never referenced, so every per-gas
        output flag the interface wrote was silently dropped.
        """
        if not ENGINE_READ_RP.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)
        self.assertIn("rpGasOriginC", _read(ENGINE_READ_RP))

    def test_undecided_flag_is_minus_one_not_zero(self):
        """0 means "off", which the engine would apply as an override.

        The record must be able to say "no opinion" so the flag is left out
        of the file and the interface falls back to the legacy flat key.
        """
        hdr = _read(RECORD_HDR)
        for field in ("outFullSp", "outFullCospW", "outRaw"):
            self.assertIn("int %s = -1;" % field, hdr)
        writer = _writer_block("INIGROUP_SCREEN_SETTINGS")
        for field in ("outFullSp", "outFullCospW", "outRaw"):
            self.assertIn("proc.%s >= 0" % field, writer)

    def test_no_fixed_output_gas_widgets_remain(self):
        hdr = _read(OUT_HDR)
        for name in ("outFullSpectraCheckBoxCo2", "outFullSpectralCheckBoxGas4",
                     "outFullCospectraCheckBoxCh4", "outRawGas4CheckBox"):
            self.assertNotIn(name + ";", hdr, "%s is a fixed gas slot" % name)
        self.assertNotIn(
            "GAS4_STRING", _read(GUI_ROOT / "src" / "defs.h"),
            "the fourth-gas label has no meaning once gases are records")


if __name__ == "__main__":
    unittest.main()


class RecordsCarryTheGasPhysics(unittest.TestCase):
    """Retiring the col_* tags removed the props the 4th gas leaned on.

    A legacy project made its fourth gas work through a rename: the column was
    relabelled 'n2o' so it would match a hard-coded species list and pick up
    N2O's unit conversion and molecular weight. With the tags retired nothing
    performs that rename, so the record has to carry the physics itself.

    Each check here corresponds to an observed failure, not a hypothetical:
    without them COS came out 1000x high, its output columns were named
    FNONE, the full output silently switched from an nmol to a umol basis,
    and every INST_LI7200_* diagnostic read -9999.
    """

    def _engine(self, *parts):
        path = ENGINE_ROOT.joinpath(*parts)
        if not path.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)
        return _read(path)

    def test_unit_conversion_is_not_keyed_on_a_species_name_list(self):
        """Anything not in the list would get no conversion at all."""
        src = self._engine("src", "src_common", "define_all_var_set.f90")
        self.assertIn(
            "RecordGasSlot(LocCol(j)%orig_col)", src,
            "DefineAllVarSet no longer asks whether a column is a "
            "record-named gas, so any species outside the literal "
            "co2/h2o/ch4/n2o list falls through to `case default` and is "
            "used in its raw file units - a silent factor of 1000 for a "
            "gas reported in ppb")
        self.assertIn(
            "call ConvertTraceGasUnits(", src,
            "the trace-gas conversion is no longer shared, so the named "
            "gases and the record-named gases can drift apart")

    def test_the_conversion_takes_the_molecular_weight_from_the_slot(self):
        """A three-way switch on the name cannot serve 64 gas slots."""
        src = self._engine("src", "src_common", "define_all_var_set.f90")
        body = src[src.index("subroutine ConvertTraceGasUnits"):]
        self.assertIn("MW(gas_slot)", body,
                      "the extracted conversion must index MW by slot")
        for name in ("MW(co2)", "MW(ch4)", "MW(gas4)"):
            self.assertNotIn(
                name, body,
                "%s is still hard-coded in the shared conversion, which "
                "binds it back to the four historical gases" % name)

    def test_molecular_weight_and_diffusivity_come_from_the_records(self):
        """MW and Dc are only `data`-initialised for slots co2:gas4.

        Every slot past the fourth gas holds whatever was in memory until
        something assigns it, and a garbage molecular weight yields a
        plausible-looking flux that is silently wrong.
        """
        src = self._engine("src", "src_common",
                           "write_processing_project_variables.f90")
        self.assertIn("MW(slot) = sngl(EddyFlowProj%gas(i)%mw)", src,
                      "gas_N_mw is read into the record but never applied "
                      "to the gas slot")
        self.assertIn("Dc(slot) = EddyFlowProj%gas(i)%diff", src,
                      "gas_N_diff is read into the record but never applied "
                      "to the gas slot")

    def test_diagnostic_records_reach_the_internal_column_slots(self):
        """Marking the column "used" is not enough for the INST_* outputs."""
        src = self._engine("src", "src_common",
                           "write_processing_project_variables.f90")
        self.assertIn("subroutine ApplyDiagnosticRecordColumns", src,
                      "nothing bridges the diag records onto "
                      "EddyFlowProj%col(E2NumVar + diag*), which is what "
                      "sets Diag7200%present and NumDiag")
        bridge = src.index("subroutine ApplyDiagnosticRecordColumns")
        clear = src.index("EddyFlowProj%col(E2NumVar + diagAnem) = nint(error)")
        call = src.index("call ApplyDiagnosticRecordColumns()")
        self.assertGreater(
            call, clear,
            "the bridge runs before the diagnostic slots are cleared to "
            "nint(error), so its assignments are wiped straight afterwards")
        self.assertTrue(bridge >= 0)

    def test_the_gas_label_and_unit_survive_the_retired_tag(self):
        """Both are consulted before ApplyGasRecords has filled E2Col.

        Named for a slot rather than for the fourth gas: the full output
        carries a column family per configured gas, so every slot needs a
        label and a unit, not just the historical fourth.
        """
        src = self._engine("src", "src_common", "define_all_var_set.f90")
        for fn in ("function GasOutputLabel(gas_slot)",
                   "function GasUnitIn(gas_slot)"):
            self.assertIn(fn, src, "%s is gone; the output headers fall back "
                                   "to an empty E2Col and the gas is named "
                                   "NONE" % fn)
        for path in (("src", "src_rp", "init_fluxnet_file_rp.f90"),
                     ("src", "src_rp", "init_outfiles_rp.f90")):
            header = self._engine(*path)
            self.assertNotIn(
                "e2sg(gas4) = E2Col(gas4)%label", header,
                "%s still reads E2Col at header time, which is before any "
                "data file has been read" % path[-1])

    def test_fcc_resolves_every_gas_column_from_the_record(self):
        """col_gas4 is retired; FCC used it to choose nmol vs umol output."""
        src = self._engine("src", "src_fcc", "read_ini_fcc.f90")
        block = src[src.index("subroutine InitializeGas4FullOutputUnitsFcc"):]
        self.assertIn(
            "gas_col = EddyFlowProj%gas(rec)%col", block,
            "FCC still takes a gas's column from the retired col_gas4 tag, "
            "so the full output silently reverts to a umol basis for a "
            "project that used nmol")
        self.assertNotIn(
            "EddyFlowProj%col(gas4)", block,
            "FCC still falls back to the retired col_gas4 slot")


class NoFourthGasConcept(unittest.TestCase):
    """The fourth slot is a gas like any other, not a species.

    It used to be a distinct row kind carrying N2O's molecular weight and
    diffusivity as literals, with a bool remembering whether the selection
    "was N2O". A site can measure any species in that slot, so those numbers
    were a guess presented as a default.
    """

    def setUp(self):
        self.src = _read(GUI_ROOT / "src" / "basicsettingspage.cpp")
        self.hdr = _read(GUI_ROOT / "src" / "basicsettingspage.h")

    def test_the_row_kinds_no_longer_name_species(self):
        for kind in ("GasCo2", "GasH2o", "GasCh4", "Gas4"):
            self.assertNotIn(
                "VariableTableRowKind::%s" % kind, self.src,
                "VariableTableRowKind::%s is back; a gas row should carry "
                "its species, not be a distinct sort of row" % kind)
        self.assertIn("VariableTableRowKind::Gas", self.src)

    def test_n2o_constants_are_not_the_open_slots_defaults(self):
        """44.01 / 0.1436 are N2O's, and were returned for the 4th slot."""
        table = self.src[:self.src.index("struct VariableTableCandidate")]
        for literal in ("44.01", "0.1436", "18.02", "16.04", "0.2178", "0.1952"):
            self.assertNotIn(
                literal, table,
                "%s is hard-coded against a row again; GasMetadata already "
                "answers this for every species" % literal)
        self.assertIn("GasMetadata::findSpecies(", self.src,
                      "molecular weight and diffusivity must be looked up "
                      "by species")

    def test_the_species_memory_is_not_a_boolean(self):
        self.assertNotIn(
            "gas4WasN2o_", self.src + self.hdr,
            "a bool can only distinguish N2O from not-N2O, so re-selecting "
            "between two non-N2O gases would not re-apply the floor")
        self.assertIn("lastAbsLimitSpecies_", self.hdr)

    def test_the_warnings_take_a_species_not_a_combo_index(self):
        for old in ("showFourthGasDiffWarning", "updateFourthGasMinLimit"):
            self.assertNotIn(old, self.src + self.hdr,
                             "%s is still slot-shaped" % old)
        for new in ("showGasDiffusivityWarning", "applyGasAbsoluteLimitMin"):
            self.assertIn(new, self.hdr, "%s is missing" % new)

    def test_the_absolute_limit_floor_belongs_to_the_species(self):
        meta = _read(GUI_ROOT / "src" / "gas_metadata.cpp")
        self.assertIn("defaultAbsoluteLimitMin", meta)
        self.assertNotIn(
            "0.032", self.src,
            "N2O's absolute-limit floor is hard-coded in the page again; it "
            "is a property of the species and belongs in GasMetadata")

    def test_the_open_slot_is_not_read_through_a_deleted_combo(self):
        """Every flux row carries combo == nullptr since the combos went.

        gasSlug() used to reach through row.row.combo->itemText() for the
        fourth slot, which is an unconditional null dereference now that the
        widgets are gone - and it is on the path data() takes for every row.
        """
        self.assertNotIn(
            "row.row.combo->itemText(", self.src,
            "the model dereferences a combo the flux rows no longer own")


class OtherGasDefaultsAreNotNamedForASlot(unittest.TestCase):
    """`*_gas4` defaults mean "a gas that is not CO2, H2O or CH4".

    The INI keys keep their spelling - a 4.x file on disk says `sr_lim_gas4`
    and the migration has to find it - but the C++ fields say what they mean.
    """

    def test_the_fields_are_renamed(self):
        state = _read(GUI_ROOT / "src" / "ecprojectstate.h")
        for old, new in (("sr_lim_gas4", "sr_lim_other"),
                         ("al_gas4_min", "al_other_min"),
                         ("al_gas4_max", "al_other_max"),
                         ("ds_hf_gas4", "ds_hf_other"),
                         ("ds_sf_gas4", "ds_sf_other"),
                         ("tl_def_gas4", "tl_def_other"),
                         ("sa_max_gas4", "sa_max_other")):
            self.assertNotIn(old, state, "%s should be %s" % (old, new))
            self.assertIn(new, state)

    def test_the_ini_keys_are_untouched(self):
        """Renaming these would orphan every setting in a 4.x project."""
        keys = _read(GUI_ROOT / "src" / "ecinidefs.h")
        for key in ("sr_lim_gas4", "al_gas4_min", "ds_hf_gas4",
                    "tl_def_gas4", "sa_max_gas4"):
            self.assertIn(
                'QStringLiteral("%s")' % key, keys,
                "the INI key %s was renamed along with the field; "
                "migrateLegacyGasSettings would stop finding it and every "
                "upgraded project would silently fall back to defaults" % key)

    def test_the_eddypro_importer_still_rewrites_to_the_ini_spelling(self):
        """The importer maps `*_n2o` onto the on-disk key, not the field.

        The pairs used to be spelled inline in the importer. They are one
        shared list now, because the SmartFlux exporter reads the same list
        backwards and the two directions must not drift; the rewrite this
        pins is unchanged.
        """
        src = _read(GUI_ROOT / "src" / "ecproject.cpp")
        for key in ("sr_lim_gas4", "al_gas4_min", "ds_hf_gas4",
                    "col_gas4", "out_full_sp_gas4"):
            self.assertIn(
                'QStringLiteral("%s")' % key, src,
                "the .eddypro importer no longer rewrites onto %s; an "
                "imported project would lose that setting entirely" % key)
        self.assertIn("fourthGasKeyRenames()", src)


class OutputColumnsAreNamedForTheirSpecies(unittest.TestCase):
    """A column name must identify what was measured, not which slot held it.

    The fourth slot used to be tagged 'GS4' when the column set was chosen and
    have its real label substituted into the finished header much later. Two
    mechanisms naming the same thing at different times is what produced a
    FLUXNET row with 20 duplicate column names.
    """

    def _engine(self, *parts):
        path = ENGINE_ROOT.joinpath(*parts)
        if not path.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)
        return _read(path)

    def _record_branch(self):
        """Executable lines of SelectFluxnetGasSlots.

        Comments are stripped because the code explains the history in prose.

        This used to cut the routine at its `else`, which named gases by fixed
        slot for a project carrying no records. That branch is gone - the
        engine refuses such a project outright - so the whole routine is the
        record branch now, and slicing at a string that is no longer there
        raised rather than skipped. A helper that throws is worse than one
        that over-matches: both tests below stopped testing anything.
        """
        src = self._engine("src", "src_rp", "init_fluxnet_file_rp.f90")
        block = src[src.index("subroutine SelectFluxnetGasSlots"):]
        block = block[: block.index("end subroutine SelectFluxnetGasSlots")]
        self.assertNotIn(
            "\n    else\n", block,
            "SelectFluxnetGasSlots has grown an else branch again; if it "
            "names gases by slot, exclude it here as this once did")
        return "\n".join(l for l in block.splitlines()
                         if not l.strip().startswith("!"))

    def test_no_slot_is_carved_out_of_species_naming(self):
        block = self._record_branch()
        self.assertNotIn(
            "HistoricGasTag(", block,
            "a slot is tagged by position again, so the duplicate check "
            "below it compares later gases against the literal 'GS4' and "
            "cannot see a repeated species")
        self.assertIn("EddyFlowProj%gas(k)%var", block,
                      "gas tags must come from the record's species")

    def test_the_duplicate_check_covers_every_slot(self):
        """Two records of one species must give H2O and H2O_2, not two H2O."""
        block = self._record_branch()
        tag_at = block.index("tag = EddyFlowProj%gas(k)%var")
        dup_at = block.index("dup = 0")
        self.assertLess(
            tag_at, dup_at,
            "the duplicate check runs before the species is known, so it "
            "cannot disambiguate anything")

    def test_the_fourth_slots_label_comes_from_the_record(self):
        """E2Col is also filled by name matching, before records are applied.

        Preferring it meant that with five gases configured the fourth slot
        could be handed the species of a different slot - the file then had
        two sets of that gas's columns and none for the gas actually in
        slot four.
        """
        src = self._engine("src", "src_common", "define_all_var_set.f90")
        body = src[src.index("function GasOutputLabel"):]
        body = body[: body.index("end function GasOutputLabel")]
        rec = body.index("EddyFlowProj%gas(rec4)%col")
        e2 = body.index("E2Col(gas_slot)%label")
        self.assertLess(
            rec, e2,
            "GasOutputLabel consults E2Col before the record again; the "
            "record is the authority on which species occupies the slot")


class SpectralAssessmentValidatorIsPositionAware(unittest.TestCase):
    """The assessment file carries one 14-row transfer-function block per
    non-water gas, so every row after the blocks shifts by 14 per gas.

    The validator spelled out three block positions - rows 19-30, 33-44 and
    47-58 - and every later section by absolute row number. Those are the CO2,
    CH4 and fourth-gas blocks only on a project laid out in that order, and
    water is skipped, so the block sequence is not the record sequence.
    """

    @classmethod
    def setUpClass(cls):
        cls.src = _read(GUI_ROOT / "src" / "ancillaryfiletest.cpp")

    def test_the_block_positions_are_computed(self):
        self.assertIn("tfpGasSlots", self.src)
        self.assertIn("kSpectraGasBlockRows", self.src)
        for literal in ("i = 32; i < 44", "i = 46; i < 58",
                        "templateList, actualList, 44, 46",
                        "templateList, actualList, 58, 62"):
            self.assertNotIn(literal, self.src,
                             "%s is an absolute row position" % literal)

    def test_water_is_excluded_from_the_blocks(self):
        """Its cutoffs are the RH-class table above them, which is why the
        engine writes one block per configured gas *but water*."""
        body = self.src[self.src.index("QVector<int> tfpGasSlots"):]
        body = body[: body.index("\n}")]
        self.assertIn('slug == QLatin1String("h2o")', body)

    def test_a_gas_count_mismatch_explains_itself(self):
        """A longer file is not a wrong file, and a bare row count says
        nothing about why it differs."""
        self.assertIn("kSpectraFixedRows", self.src)
        self.assertIn("describes %1 gases", self.src)


class CapacityConstantsMatchTheEngine(unittest.TestCase):
    """MAX_GASES gates the interface; MaxNumGases sizes the engine's arrays.

    If the interface's ceiling is the higher of the two, a project can be
    built that the engine silently truncates - extra gas records are simply
    not read, and the fluxes for them are absent rather than wrong, which is
    harder to notice.
    """

    def test_the_gas_ceiling_is_the_engine_s(self):
        engine_typedef = ENGINE_ROOT / "src" / "src_common" / "m_typedef.f90"
        if not engine_typedef.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)

        engine = _read(engine_typedef)
        m = re.search(r"integer, parameter :: MaxNumGases = (\d+)", engine)
        self.assertIsNotNone(m, "MaxNumGases not found in m_typedef.f90")

        gui = _read(GUI_ROOT / "src" / "defs.h")
        g = re.search(r"const int MAX_GASES = (\d+);", gui)
        self.assertIsNotNone(g, "MAX_GASES not found in defs.h")

        self.assertEqual(
            g.group(1), m.group(1),
            "defs.h MAX_GASES is %s but the engine's MaxNumGases is %s"
            % (g.group(1), m.group(1)))


class LegacyMigrationCoversWhatTheProjectHas(unittest.TestCase):
    """A pre-record project's thresholds have to survive the upgrade.

    migrateLegacyGasSettings demanded four gas records and returned
    otherwise, so a project whose legacy file named two or three gas columns
    kept none of its thresholds - they were read, held in the flat state, and
    dropped on the first save.
    """

    @classmethod
    def setUpClass(cls):
        src = _read(EC_PROJECT)
        start = src.index("void EcProject::migrateLegacyGasSettings")
        cls.body = src[start: src.index("\n}", start)]

    def test_it_does_not_demand_four_records(self):
        """Nor address them by position.

        It used to walk min(size, 4) and read the co2/h2o/ch4/other arrays off
        those indices, which was only ever right while migration padded the
        list. Every record is a real gas now, so the slot a flat key belongs to
        is a question about the species.
        """
        self.assertNotIn("gases.size() < 4) { return; }", self.body)
        self.assertNotIn("std::min<int>(gases.size(), 4)", self.body)
        self.assertIn("legacySlotOf", self.body)
        self.assertIn("for (int i = 0; i < gases.size(); ++i)", self.body)

    def test_water_is_skipped_by_species(self):
        """The three latent-heat settings are not per-gas. Skipping index 1
        instead assumes water sits there - true after migration, but not for a
        project that has only CO2 and CH4, where index 1 is CH4."""
        self.assertNotIn("if (i == 1) { continue; }", self.body)
        self.assertIn('gases.at(i).slug == QLatin1String("h2o")', self.body)
