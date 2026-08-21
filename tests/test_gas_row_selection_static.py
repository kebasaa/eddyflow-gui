"""The interface can build the projects the engine can process.

Two places where it could not.

**CH4 from a QCL.** The CH4 row pruned its candidates to the LI-7700, an open
path, or an instrument whose label said "Generic". The engine has never agreed:
MetadataFileValidation accepts CH4 from the MIRO MGA series, the Aerodyne
TILDAS and the Campbell EC155 and TGA200A, and this interface offers all of
them as instruments. Their display strings contain none of those three words,
so every CH4 column from a QCL or OA-ICOS analyser was dropped from the row -
and could not be rescued through the open row either, which is reached only
after CH4 has already claimed the column. This project's own regression
fixtures run CH4 on a MIRO, so the fixtures described a project the interface
could not have produced.

CH4 now follows the same rule as CO2 and H2O: an open-path analyser reports a
molar density, so a mole fraction or mixing ratio from one is rejected, and
everything else is allowed.

**The open-species controls.** The molecular-weight and diffusivity spin boxes
addressed record three unconditionally, so on a project with a fifth gas they
edited the fourth while appearing to describe the current one.
"""

from pathlib import Path
import re
import unittest


GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"

PAGE = GUI_ROOT / "src" / "basicsettingspage.cpp"


def _read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def _prune(role):
    """The predicate body of one pruneCandidates call."""
    src = _read(PAGE)
    start = src.index("pruneCandidates(VariableTableRole::%s" % role)
    return src[start:src.index("});", start)]


class Ch4AcceptsAnyClosedPathAnalyser(unittest.TestCase):
    def test_the_rule_is_the_same_as_for_co2_and_h2o(self):
        ch4 = _prune("Ch4")
        self.assertIn("isOpenPath(t) && isNotDensity(t)", ch4,
                      "CH4 must be pruned on measure type alone, as CO2 and "
                      "H2O are")

    def test_the_instrument_whitelist_is_gone(self):
        ch4 = _prune("Ch4")
        self.assertNotIn(
            "genericStr", ch4,
            "requiring the label to say Generic drops CH4 from every QCL and "
            "OA-ICOS analyser the engine accepts")

    def test_the_engine_accepts_the_analysers_this_now_allows(self):
        """The disagreement, stated against the engine rather than assumed."""
        val = ENGINE_ROOT / "src" / "src_common" / "metadata_file_validation.f90"
        if not val.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)
        src = _read(val)
        for model in ("miro_mga4_6", "aerodyne_tildas", "csi_ec155"):
            self.assertIn(
                model, src,
                "the engine no longer lists %s, so this check's premise has "
                "changed" % model)


class TheOpenGasControlsFollowTheOpenGas(unittest.TestCase):
    def test_the_record_is_found_not_hardcoded(self):
        src = _read(PAGE)
        body = src[src.index("int BasicSettingsPage::openGasRecordIndex"):]
        body = body[:body.index("\n}")]
        self.assertNotIn(
            "kLegacyGasSlots - 1", body,
            "the molecular-weight and diffusivity controls address the fourth "
            "legacy slot again, so on a project with a fifth gas they edit the "
            "wrong record")
        self.assertIn("gases.at(i).slug", body)

    def test_the_species_helper_uses_the_same_record(self):
        """Two answers to 'which open gas' is how they came apart."""
        src = _read(PAGE)
        body = src[src.index("QString BasicSettingsPage::openGasSpecies"):]
        body = body[:body.index("\n}")]
        self.assertIn("openGasRecordIndex()", body)


class TheTooltipMatchesTheFeature(unittest.TestCase):
    def test_it_no_longer_denies_repeated_species(self):
        src = _read(PAGE)
        self.assertNotIn(
            "Only one measurement per gas can be selected", src,
            "the tooltip denies something the interface has supported since "
            "gas records landed")


class TheNoHumidityWarningMirrorsTheEngine(unittest.TestCase):
    """Warning 104, shown before a run rather than found in the log after."""

    def test_it_is_gated_on_all_three_conditions(self):
        src = _read(PAGE)
        body = src[src.index("void BasicSettingsPage::showNoHumidityWarning"):]
        body = body[:body.index("\n}\n")]
        self.assertIn("gasColumns().isEmpty()", body,
                      "an anemometer-only project has no WPL to bias and must "
                      "not be warned")
        self.assertIn('QLatin1String("h2o")', body)
        self.assertIn("biomParamColRh()", body,
                      "a biomet RH sensor supplies the correction, so a site "
                      "with one must not be warned - this is the condition a "
                      "later edit is most likely to drop")

    def test_the_message_names_what_the_engine_names(self):
        src = _read(PAGE)
        body = src[src.index("void BasicSettingsPage::showNoHumidityWarning"):]
        body = body[:body.index("\n}\n")]
        for phrase in ("dry air", "buoyancy", "biomet"):
            self.assertIn(phrase, body.lower(),
                          "the dialog should say the same thing as warning 104")

    def test_the_engine_still_warns_too(self):
        """Command-line users see nothing of this dialog."""
        exc = ENGINE_ROOT / "src" / "src_common" / "exception_handler.f90"
        if not exc.is_file():
            self.skipTest("engine checkout not found at %s" % ENGINE_ROOT)
        self.assertIn("case(104)", _read(exc))


if __name__ == "__main__":
    unittest.main()
