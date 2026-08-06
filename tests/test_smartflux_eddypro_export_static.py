"""A SmartFlux package is an EddyPro project, not one of ours.

The module runs LI-COR's embedded EddyPro. It knows nothing of the measurement
records this program replaced the flat per-slot keys with, and nothing of the
features this fork added - so a verbatim copy of the native file hands it a
project with no gas columns in it at all. Every symptom of that is silent: the
package builds, the module runs, and the fluxes are computed from whatever
EddyPro assumes when col_co2 is missing.

smartflux_example/ holds a package EddyPro 7.0.9 produced, and it is the
authority here rather than anything inferred from source. It settles three
things that guesswork got wrong:

  - fluxnet_standardize_biomet, fluxnet_err_label, wdf_apply and the ru_* trio
    in [Project] all look like this fork's additions and are not. EddyPro
    writes every one. Stripping them would have been a silent downgrade.
  - the fourth slot is spelled n2o in nine keys and gas4 in the rest, and only
    the reference says which is which.
  - the package holds ini/processing.eddypro and nothing else - no metadata.

Absence from the reference does not by itself prove a key is unknown to
EddyPro: several are written only when a feature is on (pf_sect_1_* is such a
false positive). The sound test, and the one behind every removal below, is
that this program writes the key unconditionally and the reference lacks it.
"""

import re
import unittest
import zipfile
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
EC_PROJECT = GUI_ROOT / "src" / "ecproject.cpp"
EC_PROJECT_HDR = GUI_ROOT / "src" / "ecproject.h"
SMARTFLUX_BAR = GUI_ROOT / "src" / "smartfluxbar.cpp"
PROCESSING_PAGE = GUI_ROOT / "src" / "advprocessingoptions.cpp"
PROCESSING_HDR = GUI_ROOT / "src" / "advprocessingoptions.h"
ADVANCED_PAGE = GUI_ROOT / "src" / "advancedsettingspage.cpp"
OUTPUT_PAGE = GUI_ROOT / "src" / "advoutputoptions.cpp"
DEFS = GUI_ROOT / "src" / "defs.h"
#: The package EddyPro 7.0.9 produced. Read from inside the archive rather than
#: from a copy beside it: the zip is the artefact a module is given, so its
#: ini/processing.eddypro is the only spelling of the reference that cannot
#: drift from what was actually shipped.
REFERENCE_PACKAGE = GUI_ROOT / "smartflux_example" / "test.smartflux"
REFERENCE_MEMBER = "ini/processing.eddypro"

#: Slot suffixes, per group. The fourth slot is the one that differs.
PARAM_SLOTS = ("co2", "h2o", "ch4", "n2o")
SPEC_SLOTS = ("co2", "h2o", "ch4", "gas4")
OUT_SP_SLOTS = ("co2", "h2o", "ch4", "n2o")
OUT_RAW_SLOTS = ("co2", "h2o", "ch4", "gas4")
TO_SLOTS = ("co2", "h2o", "ch4", "gas4")

#: Water takes none of these: they are the latent-heat thresholds and
#: to_le_min_flux, which belong to the project rather than to a gas.
NOT_FOR_WATER = ("sa_min_st_%s", "sa_min_un_%s", "sa_max_%s", "to_%s_min_flux")


def _read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def _reference_text():
    with zipfile.ZipFile(REFERENCE_PACKAGE) as archive:
        return archive.read(REFERENCE_MEMBER).decode("utf-8", "replace")


def _reference_keys():
    """Every key in the reference package, by name (group ignored)."""
    keys = set()
    for line in _reference_text().splitlines():
        line = line.strip()
        if not line or line.startswith((";", "[")) or "=" not in line:
            continue
        keys.add(line.split("=", 1)[0])
    return keys


def _exporter_body():
    src = _read(EC_PROJECT)
    body = src[src.index("void EcProject::writeEddyProCompatibleKeys("):]
    return body[: body.index("\n}\n")]


class TheExporterEmitsOnlyKeysEddyProKnows(unittest.TestCase):
    """The reference, used as a specification rather than as a document.

    Every per-slot key the exporter builds is expanded here and looked up in
    the package EddyPro produced. A slot suffix spelled the wrong way - gas4
    where EddyPro writes n2o - is invisible in review and fatal on the module,
    and this is what catches it.
    """

    def setUp(self):
        self.reference = _reference_keys()

    def assertEmitted(self, key):
        self.assertIn(key, self.reference,
                      "%s is not a key EddyPro 7.0.9 writes" % key)

    def test_gas_columns(self):
        for key in ("col_co2", "col_h2o", "col_ch4", "col_n2o"):
            self.assertEmitted(key)

    def test_cell_and_diagnostic_columns(self):
        for key in ("col_int_t_1", "col_int_t_2", "col_int_p", "col_cell_t",
                    "col_diag_75", "col_diag_72", "col_diag_77",
                    "col_diag_anem", "gas_mw", "gas_diff"):
            self.assertEmitted(key)

    def test_screening_thresholds(self):
        for slot in PARAM_SLOTS:
            for tmpl in ("sr_lim_%s", "al_%s_min", "al_%s_max",
                         "ds_hf_%s", "ds_sf_%s", "tl_def_%s"):
                self.assertEmitted(tmpl % slot)

    def test_spectral_windows(self):
        for slot in SPEC_SLOTS:
            for tmpl in ("sa_fmin_%s", "sa_fmax_%s", "sa_hfn_%s_fmin"):
                self.assertEmitted(tmpl % slot)

    def test_the_qa_qc_triple_skips_water(self):
        for slot in SPEC_SLOTS:
            for tmpl in ("sa_min_st_%s", "sa_min_un_%s", "sa_max_%s"):
                if slot == "h2o":
                    self.assertNotIn(
                        tmpl % slot, self.reference,
                        "water's threshold is the latent-heat one, not %s"
                        % (tmpl % slot))
                else:
                    self.assertEmitted(tmpl % slot)

    def test_output_selections(self):
        for slot in OUT_SP_SLOTS:
            self.assertEmitted("out_full_sp_%s" % slot)
            self.assertEmitted("out_full_cosp_w_%s" % slot)
        for slot in OUT_RAW_SLOTS:
            self.assertEmitted("out_raw_%s" % slot)

    def test_time_lag_windows(self):
        for slot in TO_SLOTS:
            self.assertEmitted("to_%s_min_lag" % slot)
            self.assertEmitted("to_%s_max_lag" % slot)
            if slot == "h2o":
                self.assertNotIn("to_h2o_min_flux", self.reference)
            else:
                self.assertEmitted("to_%s_min_flux" % slot)

    def test_month_grouping_placeholder(self):
        for slot in ("co2", "ch4", "gas4"):
            self.assertEmitted("sa_%s_g1_start" % slot)
            self.assertEmitted("sa_%s_g1_stop" % slot)


class TheSourceMatchesThatSpecification(unittest.TestCase):
    """The exporter really does build the keys checked above."""

    def setUp(self):
        self.body = _exporter_body()

    def test_slot_suffix_tables(self):
        for name, slots in (("paramSlot", PARAM_SLOTS),
                            ("specSlot", SPEC_SLOTS),
                            ("outSpSlot", OUT_SP_SLOTS),
                            ("outRawSlot", OUT_RAW_SLOTS),
                            ("toSlot", TO_SLOTS)):
            expected = ", ".join('"%s"' % s for s in slots)
            self.assertIn(
                "%s[4] = { %s }" % (name, expected), self.body,
                "%s must be spelled %s" % (name, expected))

    def test_water_is_skipped_by_slug_not_by_index(self):
        # A project with only CO2 and CH4 has no water at index one, and
        # skipping that index would drop CH4's values instead.
        self.assertIn('slug == QLatin1String("h2o")', _read(EC_PROJECT))
        self.assertEqual(
            3, self.body.count("if (isWater[i]) { continue; }"),
            "three places skip water: the QA/QC triple, the month grouping "
            "(classed by relative humidity, so it never had a table) and the "
            "minimum flux")

    def test_the_four_eddypro_slots_are_re_pinned_by_species(self):
        """col_co2 holds CO2, col_h2o H2O, col_ch4 CH4, col_n2o everything else.

        EddyPro's layout is four fixed slots; this program's record list is
        only the gases the site measures, in selection order. The two used to
        coincide because the list padded the absent species, which is why this
        code could walk it positionally. It no longer does - a compacted list
        would file COS under `ch4` and hand the module a methane flux - so the
        pinning is rebuilt here, and only here.
        """
        self.assertIn('const char* pinned[3] = { "co2", "h2o", "ch4" };',
                      self.body)
        self.assertIn("slotOf", self.body)
        self.assertNotIn("gases.at(i).proc", self.body,
                         "the loops must go through the slot table, not the "
                         "record position")

    def test_an_unfilled_slot_still_writes_defaults(self):
        # The reference package carries every slot's keys even for gases the
        # project does not have, so an empty slot needs sentinel settings
        # rather than being skipped.
        self.assertIn("kUnfilledSlot", self.body)
        self.assertIn("procFor(", self.body)

    def test_undecided_records_fall_back_to_the_shown_default(self):
        # EddyPro would otherwise assume something for a missing key, and the
        # package is supposed to reproduce what the interface displayed.
        self.assertIn("defaultSettings.screenParam", self.body)
        self.assertIn("defaultSettings.spectraSettings", self.body)
        self.assertIn("defaultSettings.timelagOpt", self.body)
        self.assertIn("defaultSettings.screenSetting", self.body)


class TheForkOnlyKeysAreRemoved(unittest.TestCase):
    def setUp(self):
        self.body = _exporter_body()

    def test_removals(self):
        for pattern in (r'"^cec_"',
                        r'"^(gas|cell|diag)_(num$|\\d+_)"',
                        r'"^gas_\\d+_"',
                        r'"^gas_\\d+_sa_"',
                        r'"^gas_\\d+_out_"',
                        r'"^gas_\\d+_to_"'):
            self.assertIn(pattern, self.body,
                          "%s is written by this program and not by EddyPro"
                          % pattern)
        for constant in ("INI_SPEC_SETTINGS_52",   # flux_run_mode
                         "INI_SPEC_SETTINGS_53",   # automatic_spectra_config
                         "INI_SCREEN_TILT_14",     # rot_pf_assessment_only
                         "INI_TIMELAG_OPT_21",     # tlag_assessment_only
                         "INIGROUP_PWB_TIMELAG"):
            self.assertIn(constant, self.body)

    def test_the_run_mode_keys_are_named_not_just_numbered(self):
        """The three keys behind the Assessment File Outputs block.

        Pinned by name as well as by constant: the constants are positional
        (INI_SPEC_SETTINGS_52 says nothing about what it holds), so a
        renumbering in ecinidefs.h could move the removal onto a different key
        without any test noticing.
        """
        defs = _read(GUI_ROOT / "src" / "ecinidefs.h")
        for constant, key in (("INI_SPEC_SETTINGS_52", "flux_run_mode"),
                              ("INI_SCREEN_TILT_14", "rot_pf_assessment_only"),
                              ("INI_TIMELAG_OPT_21", "tlag_assessment_only")):
            self.assertIn(
                '%s    = QStringLiteral("%s")' % (constant, key),
                re.sub(r" +=", "    =", defs),
                "%s no longer holds %s, so the exporter removes the wrong key"
                % (constant, key))

    def test_the_keys_eddypro_does_write_are_kept(self):
        """The four the reference disproved.

        Each looks like this fork's work and is not. This is the regression
        most likely to be reintroduced by someone reading only the source.
        """
        reference = _reference_keys()
        for key in ("fluxnet_standardize_biomet", "fluxnet_err_label",
                    "wdf_apply", "ru_meth", "ru_its_meth", "ru_tlag_max"):
            self.assertIn(key, reference)
        # These three are simply left alone.
        for key in ("fluxnet_standardize_biomet", "fluxnet_err_label",
                    "wdf_apply"):
            self.assertNotIn(key, self.body,
                             "%s is an EddyPro key and must survive" % key)

    def test_random_error_is_written_where_eddypro_reads_it(self):
        """ru_* is not removed - it is relocated, and only if it needs to be.

        EddyPro reads the three from [Project], which is where this program
        writes them too. But they lived in a RawProcess group for years, and a
        project file not saved since still has them there; a transform that
        only copied the file forward would inherit that layout. Writing them
        from the project rather than from the file makes the export the same
        whatever the input's history - which is exactly what a round trip
        against the reference caught.
        """
        self.assertIn("INIGROUP_RAND_ERROR", self.body)
        self.assertIn("INIGROUP_RAND_ERROR_LEGACY", self.body)
        for constant in ("INI_RAND_ERROR_0", "INI_RAND_ERROR_1",
                         "INI_RAND_ERROR_2"):
            self.assertIn(constant, self.body)
        self.assertIn("randomError", self.body,
                      "the values must come from the project, not the file")


class TheFourthSlotRenameHasOneSource(unittest.TestCase):
    """Import reads the list forwards, export removes by it backwards."""

    def setUp(self):
        self.src = _read(EC_PROJECT)

    def test_the_list_is_shared(self):
        body = self.src[self.src.index("fourthGasKeyRenames()"):]
        for pair in (("col_n2o", "col_gas4"),
                     ("out_full_sp_n2o", "out_full_sp_gas4"),
                     ("out_full_cosp_w_n2o", "out_full_cosp_w_gas4"),
                     ("sr_lim_n2o", "sr_lim_gas4"),
                     ("ds_hf_n2o", "ds_hf_gas4"),
                     ("ds_sf_n2o", "ds_sf_gas4"),
                     ("al_n2o_min", "al_gas4_min"),
                     ("al_n2o_max", "al_gas4_max"),
                     ("tl_def_n2o", "tl_def_gas4")):
            self.assertIn('QStringLiteral("%s")' % pair[0], body)
            self.assertIn('QStringLiteral("%s")' % pair[1], body)

    def test_the_import_no_longer_carries_its_own_copy(self):
        body = self.src[self.src.index("EcProject::importEddyProProject("):]
        body = body[: body.index("\n}\n")]
        self.assertIn("fourthGasKeyRenames()", body)
        self.assertNotIn("col_gas4=", body,
                         "the import must not hold a second copy of the list")


class ThePackageShape(unittest.TestCase):
    def setUp(self):
        self.src = _read(SMARTFLUX_BAR)

    def test_the_project_is_exported_not_copied(self):
        self.assertIn("exportEddyProProject(", self.src)
        self.assertIn("SMARTFLUX_PROCESSING_FILENAME", self.src)
        self.assertNotIn("projectFileForcedCopy", self.src,
                         "a verbatim copy is the native format, which the "
                         "module cannot read")

    def test_no_metadata_is_packaged(self):
        # EddyPro's own package holds ini/processing.eddypro and nothing else.
        self.assertNotIn("METADATA_FILE_EXT", self.src)

    def test_creation_is_refused_when_the_project_overflows(self):
        self.assertIn("smartfluxBlockReason()", self.src)
        head = self.src[self.src.index("void SmartFluxBar::createPackage()"):]
        head = head[: head.index("setGeneralRunFcc")]
        self.assertIn("smartfluxBlockReason()", head,
                      "the check must run before anything is written")

    def test_the_declared_versions_are_eddypros(self):
        defs = _read(DEFS)
        self.assertIn('SMARTFLUX_SW_VERSION_STR = QStringLiteral("7.0.9")', defs)
        self.assertIn('SMARTFLUX_INI_VERSION_STR = QStringLiteral("4.5.0")', defs)
        self.assertIn('EDDYPRO_PD_INI_TAG', defs)
        self.assertIn("EDDYPRO_PD_INI_TAG", _read(EC_PROJECT))


class TheUnrunnableOptionsAreGreyedOut(unittest.TestCase):
    def setUp(self):
        self.src = _read(PROCESSING_PAGE)

    def test_the_page_has_smartflux_handling(self):
        self.assertIn("void AdvProcessingOptions::setSmartfluxUI()", self.src)
        self.assertIn("void setSmartfluxUI();", _read(PROCESSING_HDR))

    def test_it_is_reached_from_the_dispatcher(self):
        self.assertIn("processingOptions()->setSmartfluxUI()",
                      _read(ADVANCED_PAGE))

    def test_cec_cannot_be_switched_back_on(self):
        # refresh() and updateCecMeth_1() both re-derive availability, so the
        # guard belongs there rather than only in setSmartfluxUI.
        body = self.src[self.src.index("void AdvProcessingOptions::updateCecAvailability()"):]
        body = body[: body.index("\n}\n")]
        self.assertIn("configState_->project.smartfluxMode", body)

    def test_the_pwb_entry_is_marked_unselectable(self):
        body = self.src[self.src.index("void AdvProcessingOptions::setSmartfluxUI()"):]
        body = body[: body.index("\n}\n")]
        self.assertIn('setItemData(4', body)
        self.assertIn('QStringLiteral("disabled")', body)
        self.assertIn("currentIndex() == 4", body,
                      "a project already saved with the method set leaves the "
                      "combo sitting on it")

    def test_the_marker_has_a_model_that_honours_it(self):
        # setItemData alone does nothing: CustomComboModel is what turns the
        # marker into an unselectable item.
        self.assertIn("timeLagMethodCombo->setModel(new CustomComboModel",
                      self.src)
        combo = self.src.index("timeLagMethodCombo = new QComboBox")
        model = self.src.index("timeLagMethodCombo->setModel(")
        first_item = self.src.index('timeLagMethodCombo->addItem(')
        self.assertLess(combo, model)
        self.assertLess(model, first_item,
                        "setModel replaces the model, so it must precede the "
                        "items")


class TheRunModeIsForcedToDefault(unittest.TestCase):
    """A module computes fluxes; it does not build assessment files.

    Four of the five controls in the Assessment File Outputs block ask for an
    assessment-file run. The fifth - "Default, all options available" - is the
    only one that describes what a module does, so it is selected and left
    live while the other four go dead.

    The enabled state belongs to the two availability functions rather than to
    setSmartfluxUI, because refresh() and every time-lag and rotation-method
    slot call them: a disable applied once on entering the mode is undone by
    the first of those to fire.
    """

    RUN_MODE_WIDGETS = ("defaultRunRadioButton",
                        "spectralAssessmentCreationRadioButton",
                        "productionRunRadioButton",
                        "timelagAssessmentOnlyCheckBox",
                        "planarFitAssessmentOnlyCheckBox")

    def setUp(self):
        self.src = _read(OUTPUT_PAGE)

    def _body(self, signature):
        body = self.src[self.src.index(signature):]
        return body[: body.index("\n}\n")]

    def test_the_default_is_selected_and_stays_live(self):
        body = self._body(
            "void AdvOutputOptions::updateSpectralAssessmentCreationAvailability()")
        self.assertIn("configState_->project.smartfluxMode", body)
        self.assertIn("defaultRunRadioButton->setEnabled(true)", body)
        self.assertIn("defaultRunRadioButton->setChecked(true)", body)
        self.assertIn("setSpectraFluxRunMode(0)", body)

    def test_the_other_two_radios_go_dead(self):
        body = self._body(
            "void AdvOutputOptions::updateSpectralAssessmentCreationAvailability()")
        smartflux = body[body.index("smartfluxMode"):]
        smartflux = smartflux[: smartflux.index("return;")]
        self.assertIn("spectralAssessmentCreationRadioButton->setEnabled(false)",
                      smartflux)
        self.assertIn("productionRunRadioButton->setEnabled(false)", smartflux)

    def test_the_assessment_only_boxes_cannot_re_enable_themselves(self):
        # They follow the time-lag and rotation methods, so any change to
        # either re-derives them. The guard has to live here.
        body = self._body(
            "void AdvOutputOptions::updatePreprocessingAssessmentAvailability()")
        self.assertIn("configState_->project.smartfluxMode", body)

    def test_none_of_the_five_is_blanket_disabled(self):
        body = self._body("void AdvOutputOptions::setSmartfluxUI()")
        listed = body[body.index("QWidgetList enableableWidgets"):]
        listed = listed[: listed.index("for (auto w : enableableWidgets)")]
        for widget in self.RUN_MODE_WIDGETS:
            self.assertNotIn(
                widget, listed,
                "%s is owned by the availability functions; listing it here "
                "restores it by position from a vector that is never cleared"
                % widget)

    def test_entering_and_leaving_re_derives_them(self):
        body = self._body("void AdvOutputOptions::setSmartfluxUI()")
        self.assertIn("updateSpectralAssessmentCreationAvailability()", body)


class TheOverflowCheck(unittest.TestCase):
    def setUp(self):
        src = _read(EC_PROJECT)
        body = src[src.index("QString EcProject::smartfluxBlockReason()"):]
        self.body = body[: body.index("\n}\n")]

    def test_it_covers_every_way_the_format_can_overflow(self):
        self.assertIn("kEddyProGasSlots", self.body)
        self.assertIn("cellColumns", self.body)
        self.assertIn("tlag_meth == 5", self.body)
        self.assertIn("cec_meth != 0", self.body)

    def test_it_is_declared(self):
        self.assertIn("QString smartfluxBlockReason() const;",
                      _read(EC_PROJECT_HDR))


if __name__ == "__main__":
    unittest.main()
