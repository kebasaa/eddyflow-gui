"""The two sonic hardware corrections agree across the interface and the engine.

Eight keys in ``RawProcess_Settings``, because only RP touches raw wind. The
interface's job is threefold: to make the numbers mean what the engine reads
them to mean, to grey what a given mode does not read, and to say what these
corrections are for - "inclinometer tilt correction" tells nobody why a planar
fit is not already enough.

Three things are pinned hardest.

**Zero is off, and the combo carries the mode minus one.** Both keys are
three-valued, so the usual checkbox-plus-combo pairing has to encode the
off state in the key rather than in a separate flag. A project whose key is
1 must open with the checkbox ticked and the *first* combo entry selected.

**The Metek tables are named as absent.** They are Metek GmbH's data and are
not shipped, so a user who ticks the box and walks away would otherwise get a
silent no-op. The tooltip has to say where the files come from and what
happens without them.

**The swinging mode is a known defect, reproduced deliberately.** EddyUH adds
a scalar where the physics wants a vector (``EddyUH_tiltangle.m:104``). The
tooltip has to say so, because a user comparing against EddyUH wants it and a
user who is not comparing wants to avoid it - and neither can tell from a
label reading "Position and swinging".
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_TAGS = ENGINE_ROOT / "src" / "src_rp" / "m_rp_global_var.f90"
ENGINE_READ_INI = ENGINE_ROOT / "src" / "src_rp" / "read_ini_rp.f90"
ENGINE_TILT = ENGINE_ROOT / "src" / "src_rp" / "inclinometer_tilt.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


INIDEFS = read(GUI_ROOT / "src" / "ecinidefs.h")
STATE = read(GUI_ROOT / "src" / "ecprojectstate.h")
PROJECT = read(GUI_ROOT / "src" / "ecproject.cpp")
PAGE = read(GUI_ROOT / "src" / "advprocessingoptions.cpp")

#> key -> (GUI constant, engine tag)
KEYS = {
    "tilt_sensor_meth": ("INI_SCREEN_SETTINGS_112", ("SCTags", 31)),
    "tilt_sensor_v_g": ("INI_SCREEN_SETTINGS_113", ("SNTags", 117)),
    "tilt_arm_x": ("INI_SCREEN_SETTINGS_114", ("SNTags", 118)),
    "tilt_arm_y": ("INI_SCREEN_SETTINGS_115", ("SNTags", 119)),
    "tilt_arm_z": ("INI_SCREEN_SETTINGS_116", ("SNTags", 120)),
    "tilt_lpf_s": ("INI_SCREEN_SETTINGS_117", ("SNTags", 121)),
    "head_corr_meth": ("INI_SCREEN_SETTINGS_118", ("SCTags", 32)),
    "head_corr_dir": ("INI_SCREEN_SETTINGS_119", ("SCTags", 33)),
}

engine_available = ENGINE_TAGS.is_file() and ENGINE_READ_INI.is_file()


class TheKeysReachTheEngine(unittest.TestCase):

    def test_the_gui_declares_writes_and_reads_each(self):
        for key, (const, _) in KEYS.items():
            self.assertRegex(
                INIDEFS,
                r'const auto %s\s*=\s*QStringLiteral\("%s"\)' % (const, key))
            self.assertIn("setValue(EcIni::%s" % const, PROJECT, key)
            #> Written AND read back, not written and forgotten.
            self.assertGreaterEqual(PROJECT.count("EcIni::%s" % const), 2, key)

    def test_they_are_rawprocess_keys_not_project_ones(self):
        #> Only RP touches raw wind, and FCC never sweeps the RawProcess
        #> groups - so a [Project] key here would be swept for nothing.
        head = INIDEFS[:INIDEFS.index('QStringLiteral("tilt_sensor_meth")')]
        self.assertEqual(re.findall(r"INIGROUP_(\w+)\s*=", head)[-1],
                         "SCREEN_SETTINGS")

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_holds_them_at_the_expected_slots(self):
        tags = read(ENGINE_TAGS)
        for key, (_, (table, slot)) in KEYS.items():
            self.assertRegex(
                tags,
                r"%s\(%d\)%%Label\s*/\s*'%s'\s*/" % (table, slot, key), key)


class ZeroIsOff(unittest.TestCase):
    """Both mode keys are three-valued, with off folded into the key."""

    def test_the_gui_defaults_are_off_with_eddyuhs_numbers(self):
        self.assertIn("int tilt_sensor_meth = 0;", STATE)
        self.assertIn("int head_corr_meth = 0;", STATE)
        self.assertIn("qreal tilt_sensor_v_g = 4.0;", STATE)
        self.assertIn("qreal tilt_arm_x = -1.5;", STATE)
        self.assertIn("qreal tilt_arm_y = -1.5;", STATE)
        self.assertIn("qreal tilt_arm_z = -1.5;", STATE)
        self.assertIn("qreal tilt_lpf_s = 0.0;", STATE)

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_agrees_on_all_of_them(self):
        src = read(ENGINE_READ_INI)
        self.assertIn("RPSetup%tilt_sensor_meth = 'none'", src)
        self.assertIn("RPSetup%head_corr_meth = 'none'", src)
        block = src[src.index("RPSetup%tilt_sensor_meth = 'none'"):]
        block = block[:block.index("if (SNTagFound(117))")]
        self.assertIn("RPSetup%tilt_sensor_v_g = 4d0", block)
        self.assertIn("RPSetup%tilt_arm = -1.5d0", block)
        self.assertIn("RPSetup%tilt_lpf_s = 0d0", block)

    def test_the_checkbox_writes_the_combo_index_plus_one(self):
        #> The whole encoding. Writing the index itself would make the first
        #> mode indistinguishable from off.
        for check, combo, setter in (
                ("headCorrCheckBox", "headCorrMethCombo",
                 "setScreenHeadCorrMeth"),
                ("tiltSensorCheckBox", "tiltSensorMethCombo",
                 "setScreenTiltSensorMeth")):
            block = PAGE[PAGE.index("connect(%s, &RichTextCheckBox::clicked"
                                    % check):]
            block = block[:block.index("});")]
            self.assertIn("%s->currentIndex() + 1" % combo, block)
            self.assertIn(setter, block)
            self.assertIn(": 0", block, "off must write zero")

    def test_refresh_maps_the_mode_back_to_a_valid_combo_row(self):
        #> qMax, so an off project selects the first mode rather than -1,
        #> which QComboBox reads as "nothing selected" and shows blank.
        self.assertIn("qMax(0, ecProject_->screenHeadCorrMeth() - 1)", PAGE)
        self.assertIn("qMax(0, ecProject_->screenTiltSensorMeth() - 1)", PAGE)

    def test_an_unknown_mode_falls_back_to_off(self):
        #> Not to one of the two corrections. Guessing wrong here rewrites
        #> the wind itself, which is worse than doing nothing.
        block = PROJECT[PROJECT.index("const auto readMode = [&]"):]
        block = block[:block.index("};")]
        self.assertIn("v >= 0 && v <= 2", block)
        self.assertIn(": fallback", block)

    def test_a_sensitivity_of_zero_is_refused_on_the_way_in(self):
        block = PROJECT[PROJECT.index("const double vg"):]
        block = block[:block.index("const double lpf")]
        self.assertIn("vg > 0.0", block)

    def test_a_zero_smoothing_survives_but_a_negative_one_does_not(self):
        #> Zero is the default and means "no smoothing"; a >= is required.
        block = PROJECT[PROJECT.index("const double lpf"):]
        block = block[:block.index("}")]
        self.assertIn("lpf >= 0.0", block)
        self.assertNotIn("lpf > 0.0", block)


class TheControlsSayWhatTheyAreFor(unittest.TestCase):

    def tip(self, widget):
        m = re.search(r"%s->setToolTip\(tr\((.*?)\)\);" % widget, PAGE, re.S)
        self.assertIsNotNone(m, "%s has no tooltip" % widget)
        return "".join(re.findall(r'"([^"]*)"', m.group(1)))

    def item_tip(self, combo, index):
        m = re.search(r"%s->setItemData\(%d, tr\((.*?)\), Qt::ToolTipRole\);"
                      % (combo, index), PAGE, re.S)
        self.assertIsNotNone(m, "%s item %d has no tooltip" % (combo, index))
        return "".join(re.findall(r'"([^"]*)"', m.group(1)))

    def test_every_new_control_has_one(self):
        for w in ("headCorrCheckBox", "headCorrDirBrowse",
                  "tiltSensorCheckBox", "tiltSensorVgSpin", "tiltLpfSpin"):
            self.assertTrue(self.tip(w).strip(), w)

    def test_the_tilt_tooltip_explains_why_a_rotation_is_not_enough(self):
        tip = self.tip("tiltSensorCheckBox")
        self.assertIn("<i>mean</i> tilt", tip)
        self.assertIn("<i>within</i>", tip)

    def test_it_says_where_the_angle_columns_come_from(self):
        #> There is no control for this, so the tooltip is the only place a
        #> user learns the channels are matched by name.
        tip = self.tip("tiltSensorCheckBox")
        for name in ("<i>theta</i>", "<i>phi</i>", "<i>psi</i>"):
            self.assertIn(name, tip)
        self.assertIn("Raw File Description", tip)

    def test_it_says_the_channels_hold_volts_and_not_angles(self):
        self.assertIn("output voltage", self.tip("tiltSensorCheckBox"))

    def test_it_says_psi_is_discarded(self):
        self.assertIn("psi is always zero", self.tip("tiltSensorCheckBox"))

    def test_the_swinging_mode_admits_the_defect(self):
        tip = self.item_tip("tiltSensorMethCombo", 1)
        self.assertIn("single number", tip)
        self.assertIn("cross product", tip)
        self.assertIn("EddyUH_tiltangle.m:104", tip)
        #> And says what to do about it, which is not "adjust the arm".
        self.assertIn("no arm turns a scalar into a vector", tip)

    @unittest.skipUnless(ENGINE_TILT.is_file(), "engine repo not beside the GUI")
    def test_and_the_engine_still_reproduces_it(self):
        #> If someone ever fixes the engine, this tooltip becomes a lie.
        self.assertIn("dot_product(matmul(omega, t), RPSetup%tilt_arm)",
                      read(ENGINE_TILT))

    def test_the_metek_tooltip_says_the_tables_are_not_shipped(self):
        tip = self.tip("headCorrCheckBox")
        self.assertIn("not shipped with EddyFlow", tip)
        self.assertIn("Metek GmbH", tip)
        for name in ("<i>phicorr.dat</i>", "<i>ucorr.dat</i>",
                     "<i>alphacorr.dat</i>"):
            self.assertIn(name, tip)
        #> And what happens when they are missing, so a user who ticks the
        #> box and walks away is not surprised by an unchanged answer.
        self.assertIn("declines", tip)

    def test_the_undo_mode_says_why_it_exists(self):
        tip = self.item_tip("headCorrMethCombo", 1)
        self.assertIn("count the horizontal part twice", tip)


class TheGreyingFollowsWhatEachModeReads(unittest.TestCase):

    def body(self):
        fn = re.search(
            r"void AdvProcessingOptions::updateSonicHardwareAvailability\(\)"
            r"\s*\{(.*?)\n\}", PAGE, re.S)
        self.assertIsNotNone(fn)
        return fn.group(1)

    def test_each_block_greys_with_its_own_checkbox(self):
        body = self.body()
        for w in ("headCorrMethLabel", "headCorrMethCombo", "headCorrDirLabel",
                  "headCorrDirBrowse"):
            self.assertIn("%s->setEnabled(head)" % w, body)
        for w in ("tiltSensorMethLabel", "tiltSensorMethCombo",
                  "tiltSensorVgLabel", "tiltSensorVgSpin", "tiltLpfLabel",
                  "tiltLpfSpin"):
            self.assertIn("%s->setEnabled(tilt)" % w, body)

    def test_the_lever_arm_greys_with_the_mode_not_the_checkbox(self):
        #> Plain position correction never reads it, and a live box invites a
        #> number that will be ignored.
        body = self.body()
        self.assertIn("tiltSensorMethCombo->currentIndex() == 1", body)
        for w in ("tiltArmLabel", "tiltArmXSpin", "tiltArmYSpin",
                  "tiltArmZSpin"):
            self.assertIn("%s->setEnabled(swinging)" % w, body)

    def test_changing_the_mode_re_greys_the_arm(self):
        block = PAGE[PAGE.index("connect(tiltSensorMethCombo,"):]
        block = block[:block.index("connect(tiltSensorMethLabel,")]
        self.assertIn("updateSonicHardwareAvailability()", block)

    def test_the_checkboxes_write_on_the_click_not_the_state(self):
        #> refresh() blocks the project's signals, not the widgets'.
        for w in ("headCorrCheckBox", "tiltSensorCheckBox"):
            self.assertIn("connect(%s, &RichTextCheckBox::clicked" % w, PAGE)
            self.assertNotIn("connect(%s, &RichTextCheckBox::toggled" % w,
                             PAGE)


class TheGridHasNoGaps(unittest.TestCase):
    """Five controls went into the middle of an existing page."""

    def test_the_rows_run_consecutively(self):
        rows = sorted({int(m) for m in re.findall(
            r"settingsLayout->add(?:Widget|Layout)\([A-Za-z_0-9]+, (\d+),",
            PAGE)})
        self.assertEqual(rows, list(range(rows[0], rows[-1] + 1)),
                         "a renumbering left a hole in the grid")

    def test_the_new_block_sits_between_angle_of_attack_and_rotation(self):
        def row(widget):
            m = re.search(
                r"settingsLayout->addWidget\(%s, (\d+)," % widget, PAGE)
            self.assertIsNotNone(m, widget)
            return int(m.group(1))

        self.assertLess(row("aoaCheckBox"), row("headCorrCheckBox"))
        self.assertLess(row("headCorrCheckBox"), row("tiltSensorCheckBox"))
        self.assertLess(row("tiltSensorCheckBox"), row("rotCheckBox"))


if __name__ == "__main__":
    unittest.main()
