"""Whose spectral transfer function each gas is corrected with.

A gas too noisy for its own in-situ fit - COS and N2O on CH-LAE's MIRO, at
fc 0.002 and 0.035 Hz with Fn 0.04-0.07 - can take the fit of a gas on the
same analyser, because tube, cell and sensor response belong to the analyser.
Two settings in ``[FluxCorrection_SpectralAnalysis_General]``:

  * ``gas_<i>_sa_source`` per gas: absent automatic, 0 the gas's own fit, j
    the record of a tube-mate. The record carries it as ``saSource`` (0
    automatic, ``kSaSourceOwn`` own, j donor), so every rearrangement of the
    list has to carry it as it carries ``tlagDonor``.
  * ``sa_auto_source``: whether automatic takes the suggested tube-mate when
    the gas's own fit looks implausible. Off by default, and never on in the
    run that creates the assessment (``flux_run_mode`` 1).

The interface previews the engine's rule on a selected file, so the two must
use the same thresholds; that is checked against the engine checkout beside
this one when it is there.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
SRC = GUI_ROOT / "src"
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_FCC_TAGS = ENGINE_ROOT / "src" / "src_fcc" / "m_fx_global_var_mod.f90"
ENGINE_COMMON = ENGINE_ROOT / "src" / "src_common" / "m_common_global_var.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def body(src, start, end="\n}"):
    i = src.index(start)
    return src[i:src.index(end, i)]


INIDEFS = read(SRC / "ecinidefs.h")
STATE = read(SRC / "ecprojectstate.h")
PROJECT = read(SRC / "ecproject.cpp")
PROJECT_H = read(SRC / "ecproject.h")
RECORD_H = read(SRC / "measurement_record.h")
RECORD_CPP = read(SRC / "measurement_record.cpp")
PAGE = read(SRC / "advspectraloptions.cpp")
DIALOG = read(SRC / "sasourcedialog.cpp")
FILE_H = read(SRC / "spectralassessmentfile.h")
FILE_CPP = read(SRC / "spectralassessmentfile.cpp")
CMAKE = read(GUI_ROOT / "CMakeLists.txt")


def writer_block():
    start = PROJECT.index("project_ini.beginGroup(EcIni::INIGROUP_SPEC_SETTINGS)")
    return PROJECT[start:PROJECT.index("project_ini.endGroup()", start)]


class TheRecordCarriesTheSource(unittest.TestCase):

    def test_automatic_is_the_default(self):
        self.assertIn("int saSource = 0;", RECORD_H)
        self.assertIn("constexpr int kSaSourceOwn = -1;", RECORD_H)

    def test_only_a_non_water_tube_mate_may_lend(self):
        fn = body(RECORD_CPP, "bool isSaSourceFor(")
        self.assertIn("gas.slug == kH2o || source.slug == kH2o", fn)
        self.assertIn("source.instrumentId == gas.instrumentId", fn)
        self.assertIn("isRealInstrument(gas.instrumentId)", fn)
        self.assertIn("gas.rawColumn <= 0 || source.rawColumn <= 0", fn)

    def test_a_stale_source_goes_back_to_automatic(self):
        fn = body(RECORD_CPP, "void validateSaSources(")
        self.assertIn("gases[i].saSource = 0;", fn)
        self.assertIn("validateSaSources(gases);", RECORD_CPP)

    def test_every_rearrangement_carries_it(self):
        remap = body(PROJECT, "static void remapMoistureRefs(")
        self.assertIn("if (gas.saSource > 0)", remap)
        self.assertIn("remap.at(gas.saSource)", remap)

    def test_it_does_not_invalidate_a_computed_dataset(self):
        #> The dataset identity is raw processing's; the source is FCC's
        #> alone, so changing it must leave "Flux correction only" available.
        block = body(PROJECT, "bool EcProject::gasRecordsCompare(")
        self.assertNotIn("saSource", block)


class TheKeysAreWrittenAndRead(unittest.TestCase):

    def test_the_per_gas_key_is_tri_state(self):
        w = writer_block()
        own = w[w.index("const int source = gases.at(i).saSource;"):]
        own = own[:own.index("\n            }\n") if "\n            }\n" in own
                  else own.index("\r\n            }\r\n")]
        self.assertIn("if (source == kSaSourceOwn)", own)
        self.assertIn('project_ini.setValue(p + QStringLiteral("source"), 0);', own)
        self.assertIn("MeasurementRecords::isSaSourceFor(gases, i, source - 1)", own)
        self.assertIn('project_ini.setValue(p + QStringLiteral("source"), source);', own)

    def test_the_reader_refuses_what_cannot_lend(self):
        self.assertIn('project_ini.value(p + QStringLiteral("source"))', PROJECT)
        self.assertIn("gases[i].saSource = kSaSourceOwn;", PROJECT)
        self.assertGreaterEqual(PROJECT.count("MeasurementRecords::isSaSourceFor("), 2)

    def test_the_switch_is_declared_written_and_read(self):
        self.assertRegex(
            INIDEFS,
            r'const auto INI_SPEC_SETTINGS_55\s*=\s*QStringLiteral\("sa_auto_source"\)')
        self.assertIn("setValue(EcIni::INI_SPEC_SETTINGS_55", PROJECT)
        self.assertIn("value(EcIni::INI_SPEC_SETTINGS_55", PROJECT)
        self.assertRegex(STATE, r"\bint sa_auto_source = 0;")
        self.assertIn("int spectraAutoSource() const", PROJECT_H)

    def test_the_switch_is_never_written_on_while_creating_the_assessment(self):
        w = writer_block()
        i = w.index("setValue(EcIni::INI_SPEC_SETTINGS_55")
        self.assertIn("flux_run_mode == 1", w[i:i + 200])

    def test_eddypro_export_drops_both(self):
        self.assertIn("ini.remove(EcIni::INI_SPEC_SETTINGS_55);", PROJECT)
        self.assertIn('removeMatchingKeys(ini, QStringLiteral("^gas_\\\\d+_sa_"));',
                      PROJECT)


class ThePage(unittest.TestCase):

    def test_the_helper_runs_on_every_state_path(self):
        #> reset() and refresh() end in refreshSpectralAssessmentCreationMode,
        #> which runs it on every way out.
        mode = body(PAGE, "void AdvSpectralOptions::refreshSpectralAssessmentCreationMode()")
        self.assertIn("qScopeGuard([this]{ updateSpectraSourceWidgets(); })", mode)
        for fn in ("void AdvSpectralOptions::reset()",
                   "void AdvSpectralOptions::refresh()"):
            self.assertIn("refreshSpectralAssessmentCreationMode();", body(PAGE, fn), fn)
        for fn in ("void AdvSpectralOptions::setSmartfluxUI()",
                   "void AdvSpectralOptions::spectraRadioClicked(",
                   "void AdvSpectralOptions::updateHfMethod_1(",
                   "void AdvSpectralOptions::updateHfMethod_2(",
                   "void AdvSpectralOptions::updateSpectraFile("):
            self.assertIn("updateSpectraSourceWidgets();", body(PAGE, fn), fn)

    def test_the_switch_is_off_while_creating_the_assessment(self):
        fn = body(PAGE, "void AdvSpectralOptions::updateSpectraSourceWidgets()")
        self.assertIn("const bool creatingAssessment = ecProject_->spectraFluxRunMode() == 1;", fn)
        self.assertIn("ecProject_->setSpectraAutoSource(0);", fn)
        self.assertIn("spectraAutoSourceCheck->setEnabled(inSitu && !creatingAssessment);", fn)
        self.assertIn("spectraSourcesButton->setEnabled(inSitu);", fn)

    def test_a_shared_file_is_downloaded_only_when_the_dialog_opens(self):
        self.assertIn("refreshSpectraFits(false);",
                      body(PAGE, "void AdvSpectralOptions::updateSpectraSourceWidgets()"))
        self.assertIn("refreshSpectraFits(true);",
                      body(PAGE, "void AdvSpectralOptions::showSaSourceDialog()"))

    def test_the_dialog_offers_what_the_record_admits(self):
        self.assertIn("MeasurementRecords::isSaSourceFor(gases, i, j)", DIALOG)
        self.assertIn('combo->addItem(tr("Own assessment"), kSaSourceOwn);', DIALOG)

    def test_the_new_sources_are_built(self):
        for name in ("sasourcedialog", "spectralassessmentfile"):
            self.assertIn("src/%s.h" % name, CMAKE)
            self.assertIn("src/%s.cpp" % name, CMAKE)


class TheFileIsReadTheEnginesWay(unittest.TestCase):

    def test_blocks_are_matched_by_stamp_then_name(self):
        fn = body(FILE_CPP, "int recordForHeader(")
        self.assertLess(fn.index('stampValue(words, QStringLiteral("var"))'),
                        fn.index("recordsOfSpecies(gases, name)"))
        #> Never by position: the reader keeps no block counter.
        self.assertNotIn("blockIndex", FILE_CPP)

    def test_water_blocks_are_skipped(self):
        fn = body(FILE_CPP, "Fits readFits(")
        self.assertIn('header.contains(QStringLiteral("numerosity"))', fn)

    def test_the_rule_mirrors_the_engine(self):
        usable = body(FILE_CPP, "bool fitUsable(")
        self.assertIn("fit.fc > 0.0", usable)
        self.assertIn("fit.fn >= kFnMin", usable)
        rule = body(FILE_CPP, "int suggestedSource(")
        self.assertIn("if (ref < 0 || ref == gasIndex) { return -1; }", rule)
        self.assertIn("ratio < kFcRatioMin || ratio > 1.0 / kFcRatioMin", rule)

    @unittest.skipUnless(ENGINE_COMMON.is_file(), "engine checkout not beside the GUI")
    def test_the_thresholds_are_the_engines(self):
        engine = read(ENGINE_COMMON)

        def fortran(name):
            m = re.search(r"%s\s*=\s*([0-9.]+)d0" % name, engine)
            self.assertTrue(m, "%s not found in the engine" % name)
            return float(m.group(1))

        def cpp(name):
            m = re.search(r"constexpr double %s\s*=\s*([0-9.]+);" % name, FILE_H)
            self.assertTrue(m, "%s not found in the interface" % name)
            return float(m.group(1))

        self.assertEqual(cpp("kFnMin"), fortran("SaFnMin"))
        self.assertEqual(cpp("kFcRatioMin"), fortran("SaFcRatioMin"))

    @unittest.skipUnless(ENGINE_FCC_TAGS.is_file(), "engine checkout not beside the GUI")
    def test_the_engine_holds_both_keys(self):
        tags = read(ENGINE_FCC_TAGS)
        self.assertIn("'gas_1_sa_source'", tags)
        self.assertIn("'sa_auto_source'", tags)


if __name__ == "__main__":
    unittest.main()
