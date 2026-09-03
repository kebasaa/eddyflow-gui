"""The cospectral-model choice agrees across the interface and the engine.

One key, ``cosp_model``, in ``[Project]``. The group is not a style choice:
BOTH applications read it - RP runs the analytic corrections itself - and FCC
never sweeps the ``RawProcess_*`` groups, so a ``RawProcess_Settings`` key
would silently do nothing on the correction side. ``hf_meth`` sits in the same
group for the same reason and is the precedent this follows throughout.

The combo index IS the ini value here, deliberately: no ``setItemData``
mapping, no ``index + 1``. Anything else would need the engine's decode table
mirrored in two places.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_TAGS = ENGINE_ROOT / "src" / "src_common" / "m_common_global_var.f90"
ENGINE_DECODER = (ENGINE_ROOT / "src" / "src_common"
                  / "write_processing_project_variables.f90")
ENGINE_IMPORT = ENGINE_ROOT / "src" / "src_common" / "m_eddypro_import.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


INIDEFS = read(GUI_ROOT / "src" / "ecinidefs.h")
STATE = read(GUI_ROOT / "src" / "ecprojectstate.h")
PROJECT = read(GUI_ROOT / "src" / "ecproject.cpp")
PAGE = read(GUI_ROOT / "src" / "advspectraloptions.cpp")

#: combo row -> the name the engine decodes that row to.
ROWS = {
    0: "moncrieff_97",
    1: "kaimal_72",
    2: "sakai_01",
    3: "su_03",
    4: "moraes_08",
    5: "kristensen_97",
}

engine_available = ENGINE_TAGS.is_file() and ENGINE_DECODER.is_file()


class TheKeyReachesTheEngine(unittest.TestCase):

    def test_the_gui_declares_writes_and_reads_it(self):
        self.assertRegex(
            INIDEFS,
            r'const auto INI_PROJECT_83\s*=\s*QStringLiteral\("cosp_model"\)')
        self.assertIn("setValue(EcIni::INI_PROJECT_83", PROJECT)
        self.assertIn("value(EcIni::INI_PROJECT_83", PROJECT)

    def test_it_is_a_project_key_not_a_rawprocess_one(self):
        #> The whole reason it is where it is. INI_PROJECT_* constants are
        #> written into the [Project] group; FCC reads that group and no
        #> RawProcess one, and FCC is where the corrections are applied.
        head = INIDEFS[:INIDEFS.index('QStringLiteral("cosp_model")')]
        last_group = re.findall(r"INIGROUP_(\w+)\s*=", head)[-1]
        self.assertEqual(last_group, "PROJECT")

    def test_the_default_is_moncrieff(self):
        self.assertRegex(STATE, r"\bint cosp_model = 0;")

    def test_newprojects_and_resets_take_that_default(self):
        self.assertIn(
            "ec_project_state_.projectGeneral.cosp_model = "
            "defaultEcProjectState.projectGeneral.cosp_model;", PROJECT)
        self.assertIn(
            "WidgetUtils::resetComboToItem(cospModelCombo, "
            "ecProject_->defaultSettings.projectGeneral.cosp_model);", PAGE)

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_holds_the_key_at_the_slot_it_was_given(self):
        self.assertRegex(read(ENGINE_TAGS),
                         r"EPPrjNTags\(7\)%Label\s*/\s*'cosp_model'\s*/")

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engines_absent_value_is_the_guis_default(self):
        src = read(ENGINE_DECODER)
        self.assertIn("EddyFlowProj%cosp_model = 'moncrieff_97'", src)
        self.assertEqual(ROWS[0], "moncrieff_97")


class TheComboIndexIsTheIniValue(unittest.TestCase):

    def test_the_combo_has_one_row_per_engine_case(self):
        block = PAGE[PAGE.index("cospModelCombo = new QComboBox;"):
                     PAGE.index("cospModelCombo->setItemData(0")]
        self.assertEqual(block.count("cospModelCombo->addItem("), len(ROWS))

    def test_nothing_maps_the_index_on_the_way_out(self):
        #> The engine's decode is 0..5 in order, so the row IS the value.
        #> An index+1 or a setItemData table here would be a second copy of
        #> that mapping, and the two would drift.
        self.assertIn("ecProject_->setGeneralCospModel(n);", PAGE)
        self.assertIn("cospModelCombo->setCurrentIndex("
                      "ecProject_->generalCospModel());", PAGE)
        self.assertNotIn("cospModelCombo->setItemData(0, QVariant", PAGE)

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_every_row_lands_on_the_engine_case_of_the_same_number(self):
        src = read(ENGINE_DECODER)
        block = src[src.index("EPPrjNTagFound(7)"):]
        block = block[:block.index("end select")]
        for value, name in ROWS.items():
            if value == 0:
                continue  # the default, reached through case default
            self.assertRegex(
                block,
                r"case \(%d\)\s*\r?\n\s*EddyFlowProj%%cosp_model = '%s'"
                % (value, name),
                "combo row %d does not decode to %s" % (value, name))

    def test_each_row_carries_a_citation_tooltip(self):
        for value in ROWS:
            self.assertIn("cospModelCombo->setItemData(%d," % value, PAGE)

    def test_a_value_from_a_later_version_is_not_shown_as_the_first_row(self):
        #> Loading an out-of-range value would leave setCurrentIndex a no-op
        #> and the combo showing Moncrieff while the file said something else.
        block = PROJECT[PROJECT.index("value(EcIni::INI_PROJECT_83"):]
        block = block[:block.index("newEcProject")] if "newEcProject" in block \
            else block[:2000]
        self.assertIn("cosp_model < 0", block)
        self.assertIn("cosp_model > 5", block)


class TheTooltipSaysWhatItActuallyDoes(unittest.TestCase):

    def tip(self):
        m = re.search(r"const QString cospTip = tr\((.*?)\);", PAGE, re.S)
        self.assertIsNotNone(m, "no shared tooltip")
        return "".join(re.findall(r'"([^"]*)"', m.group(1)))

    def test_it_says_this_is_a_modifier_not_a_method(self):
        #> It sits directly under a list of correction methods. Without this
        #> it reads as a sixth one.
        tip = self.tip()
        self.assertIn("modifier on whichever method is selected", tip)
        self.assertIn("not a method of its own", tip)

    def test_it_names_the_default(self):
        self.assertIn("Moncrieff et al. (1997) is the default", self.tip())

    def test_it_says_the_library_is_not_eddyuhs_method(self):
        #> EddyUH ships these as diagnostic curves and corrects against
        #> measured cospectra. Choosing one here is not reproducing
        #> EddyUH's correction, and the tooltip is where that has to be said.
        tip = self.tip()
        self.assertIn("corrects against measured and fitted cospectra rather "
                      "than these", tip)

    def test_the_neutral_models_warn_about_stable_stratification(self):
        #> Four of the six have no stability branch at all. Applied under
        #> stable conditions they understate the high-frequency loss, and
        #> nothing in the name of the option hints at it.
        m = re.search(r"const QString cospNeutral = tr\((.*?)\);", PAGE, re.S)
        self.assertIsNotNone(m)
        neutral = "".join(re.findall(r'"([^"]*)"', m.group(1)))
        self.assertIn("No stability dependence", neutral)
        self.assertIn("understate the high-frequency", neutral)
        for value in (2, 3, 4, 5):
            block = PAGE[PAGE.index("cospModelCombo->setItemData(%d," % value):]
            block = block[:block.index("Qt::ToolTipRole")]
            self.assertIn("cospNeutral", block,
                          "row %d is a neutral-form model but does not carry "
                          "the warning" % value)

    def test_the_momentum_cospectrum_is_stated_to_be_unchanged(self):
        m = re.search(r"const QString cospShared = tr\((.*?)\);", PAGE, re.S)
        shared = "".join(re.findall(r'"([^"]*)"', m.group(1)))
        self.assertIn("Reynolds stress keeps Moncrieff's", shared)


class TheStalenessRuleFollowsHfMeth(unittest.TestCase):

    def test_it_is_deliberately_absent_from_the_comparison(self):
        #> fuzzyCompare asks whether a COMPUTED DATASET is stale - the ex
        #> record RP writes. cosp_model is read at correction time, like
        #> hf_meth, which is likewise not compared. Adding it would force a
        #> full reprocessing for a setting FCC applies on its own.
        fuzzy = PROJECT[PROJECT.index("bool EcProject::fuzzyCompare"):]
        fuzzy = fuzzy[:fuzzy.index("\n}")]
        self.assertNotIn("cosp_model", fuzzy)
        self.assertNotIn(
            "projectGeneral.hf_meth == previousProject", fuzzy,
            "hf_meth has been added to the comparison; cosp_model was left "
            "out to match it and should be reconsidered")

    @unittest.skipUnless(ENGINE_IMPORT.is_file(), "engine repo not beside the GUI")
    def test_a_converted_eddypro_project_carries_it(self):
        #> The import writes a complete EddyFlow project rather than one the
        #> interface has to migrate on open.
        self.assertIn("'cosp_model", read(ENGINE_IMPORT))


if __name__ == "__main__":
    unittest.main()
