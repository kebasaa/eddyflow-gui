"""Consecutive-difference despiking agrees across the interface and the engine.

EddyUH's ``spi_method = 1``. The interface writes ``despike_vm = 2`` and a set
of absolute step limits; the engine reads them and replaces a sample that steps
further from its predecessor than the limit says.

Three things are worth pinning, and none of them is visible from either side
alone:

1. **The radio's id IS the ini value.** ``despike_vm`` already carried 0 and 1;
   the new arm is 2. A button group renumbered to keep its rows contiguous
   would silently change the method of every project that states one.
2. **The step limits are not sigma multipliers**, and the two must not be
   confused on either side. They are absolute, in each variable's own unit,
   which is why there are four sonic keys where the Vickers settings share two.
3. **Zero means "not despiked"**, on both sides, for both the sonic keys and
   the per-gas one. The engine names every column it passed over for that
   reason, so the interface must not silently substitute a default.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_TAGS = ENGINE_ROOT / "src" / "src_rp" / "m_rp_global_var.f90"
ENGINE_READER = ENGINE_ROOT / "src" / "src_rp" / "read_ini_rp.f90"
ENGINE_METHOD = ENGINE_ROOT / "src" / "src_rp" / "despike_consecutive_diff.f90"
ENGINE_GEN = ENGINE_ROOT / "prj" / "gen_project_tags.py"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


INIDEFS = read(GUI_ROOT / "src" / "ecinidefs.h")
STATE = read(GUI_ROOT / "src" / "ecprojectstate.h")
PROJECT = read(GUI_ROOT / "src" / "ecproject.cpp")
PAGE = read(GUI_ROOT / "src" / "advstatisticaloptions.cpp")
RECORD = read(GUI_ROOT / "src" / "measurement_record.h")

#: ini key -> (GUI constant, engine SNTags slot)
SONIC_KEYS = {
    "sr_step_u": ("INI_SCREEN_PARAM_61", 61),
    "sr_step_v": ("INI_SCREEN_PARAM_62", 62),
    "sr_step_w": ("INI_SCREEN_PARAM_63", 63),
    "sr_step_ts": ("INI_SCREEN_PARAM_64", 64),
}

engine_available = ENGINE_TAGS.is_file() and ENGINE_READER.is_file()


class TheMethodIdIsTheIniValue(unittest.TestCase):

    def test_the_button_group_gives_the_new_radio_id_two(self):
        self.assertIn("despikingRadioGroup->addButton(vickersDespikingRadio, 0)",
                      PAGE)
        self.assertIn("despikingRadioGroup->addButton(mauderDespikingRadio, 1)",
                      PAGE)
        self.assertIn("despikingRadioGroup->addButton(stepDespikingRadio, 2)",
                      PAGE)

    def test_the_refresh_maps_two_to_the_new_radio_and_falls_back_to_vickers(self):
        block = PAGE[PAGE.index("switch (ecProject_->screenParamDespikeVm())"):]
        block = block[:block.index("despikingRadioClicked")]
        self.assertRegex(block, r"case 1:\s*\n\s*mauderDespikingRadio")
        self.assertRegex(block, r"case 2:\s*\n\s*stepDespikingRadio")
        self.assertRegex(block, r"default:\s*\n\s*vickersDespikingRadio")

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_decodes_the_same_three(self):
        src = read(ENGINE_READER)
        block = src[src.index("select case (SCTags(90)%value(1:1))"):]
        block = block[:block.index("end select")]
        self.assertRegex(block, r"case \('0'\)[\s\S]{0,80}?'vickers_97'")
        self.assertRegex(block, r"case \('2'\)[\s\S]{0,400}?'consecutive_diff'")
        #> The historical catch-all. Anything that was not '0' meant Mauder
        #> before this arm existed, and still does.
        self.assertRegex(block, r"case default[\s\S]{0,80}?'mauder_13'")


class TheStepLimitsAreAbsolute(unittest.TestCase):

    def test_the_gui_declares_writes_and_reads_all_four(self):
        for key, (const, _) in SONIC_KEYS.items():
            self.assertRegex(
                INIDEFS,
                r'const auto %s\s*=\s*QStringLiteral\("%s"\)' % (const, key))
            self.assertIn("setValue(EcIni::%s" % const, PROJECT)
            self.assertIn("EcIni::%s" % const, PROJECT)

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_holds_them_at_the_expected_slots(self):
        tags = read(ENGINE_TAGS)
        for key, (_, slot) in SONIC_KEYS.items():
            self.assertRegex(
                tags, r"SNTags\(%d\)%%Label\s*/\s*'%s'\s*/" % (slot, key),
                "the engine does not hold %r at SNTags(%d)" % (key, slot))

    def test_there_are_four_and_not_two(self):
        #> The Vickers settings share sr_lim_u across u, v and Ts because a
        #> sigma multiplier is dimensionless. These are not: a kelvin and a
        #> metre per second cannot sit behind the same number.
        self.assertEqual(len(SONIC_KEYS), 4)
        for f in ("sr_step_u", "sr_step_v", "sr_step_w", "sr_step_ts"):
            self.assertIn(f, STATE)

    def test_the_defaults_are_the_ch_lae_values(self):
        #> EddyUH's own dlim for the project this was built to reproduce.
        self.assertRegex(STATE, r"qreal sr_step_u = 10\.0;")
        self.assertRegex(STATE, r"qreal sr_step_v = 10\.0;")
        self.assertRegex(STATE, r"qreal sr_step_w = 5\.0;")
        self.assertRegex(STATE, r"qreal sr_step_ts = 10\.0;")

    def test_the_page_never_scales_a_step_by_a_sigma(self):
        #> The absolute limits on this page are unit-scaled on the way in and
        #> out (absLimScale). A step limit must not be: it is already in the
        #> column's own unit, and scaling it would silently move it by three
        #> orders for a gas reported in ppb.
        block = PAGE[PAGE.index("case GasParam::StepLim: gases[gasIndex]"):]
        block = block[:block.index("break;") + 6]
        self.assertNotIn("scale.factor", block)


class ZeroMeansNotDespiked(unittest.TestCase):

    def test_the_per_gas_sentinel_is_negative_and_the_default_is_zero(self):
        self.assertRegex(RECORD, r"qreal stepLim = -1\.0;")
        #> An absolute limit cannot be guessed from the species: the unit is a
        #> property of the column. Zero is the only safe assumption.
        block = PAGE[PAGE.index("case GasParam::StepLim:"):]
        block = block[:block.index("return 0.0;") + 11]
        self.assertIn("return 0.0;", block)

    def test_the_spins_say_so_rather_than_showing_a_bare_zero(self):
        self.assertGreaterEqual(
            PAGE.count('setSpecialValueText(tr("not despiked"))'), 2,
            "the sonic spins and the per-gas spin should both say what zero "
            "means")

    def test_a_negative_sonic_limit_falls_back_rather_than_being_kept(self):
        block = PROJECT[PROJECT.index("const auto readStep = "):]
        block = block[:block.index("sr_step_ts") + 40]
        self.assertIn("v >= 0.0 ? v : fallback", block)

    @unittest.skipUnless(ENGINE_METHOD.is_file(), "engine repo not beside the GUI")
    def test_the_engine_skips_and_names_such_a_column(self):
        src = read(ENGINE_METHOD)
        self.assertIn("if (step_lim(j) <= 0d0) then", src)
        self.assertIn("No step limit stated for:", src)


class TheSettingInvalidatesAComputedDataset(unittest.TestCase):

    def test_the_method_and_all_four_limits_are_compared(self):
        #> Despiking changes the series every later stage is computed from, so
        #> a change to any of these makes a previous run's output stale.
        fuzzy = PROJECT[PROJECT.index("bool EcProject::fuzzyCompare"):]
        fuzzy = fuzzy[:fuzzy.index("\n}")]
        self.assertIn("screenParam.despike_vm", fuzzy)
        for f in ("sr_step_u", "sr_step_v", "sr_step_w", "sr_step_ts"):
            self.assertIn("screenParam.%s" % f, fuzzy)

    @unittest.skipUnless(ENGINE_GEN.is_file(), "engine repo not beside the GUI")
    def test_the_per_gas_key_is_appended_to_the_engines_list(self):
        #> RP_GAS_NUMERIC is positional. Inserting anywhere but the end
        #> re-points every later per-gas setting of every gas.
        gen = read(ENGINE_GEN)
        block = gen[gen.index("RP_GAS_NUMERIC = ["):gen.index("RP_GAS_TEXT")]
        self.assertRegex(block, r'\+ \["step_lim"\]\s*$')


if __name__ == "__main__":
    unittest.main()
