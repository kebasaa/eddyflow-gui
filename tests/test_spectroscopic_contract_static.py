"""The spectroscopic correction agrees across the interface and the engine.

Two halves have to line up, and neither failure mode raises.

**The project settings.** ``spectro_meth`` and ``spectro_water`` are written
into ``[RawProcess_Settings]``, which the engine sweeps with the prefix
``RawProcess``. A key spelled differently on the two sides is simply not
found and the correction never runs.

**The per-column coefficients.** These live in the *metadata* file, not the
project, and go through a different generator. They must not collide with
``a_value`` and ``b_value``, which are the linear calibration gain and offset:
the engine matches tag names exactly and forbids one label being a substring
of another, and a reader would confuse the two anyway.

Both must default to the identity - method off, coefficients zero - so that a
project and a metadata file written before any of this existed keep their
numbers.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_TAGS = ENGINE_ROOT / "src" / "src_rp" / "m_rp_global_var.f90"
ENGINE_READER = ENGINE_ROOT / "src" / "src_rp" / "read_ini_rp.f90"
ENGINE_META_TAGS = ENGINE_ROOT / "src" / "src_common" / "m_common_global_var.f90"
ENGINE_META_READ = ENGINE_ROOT / "src" / "src_common" / "read_metadata_file.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


INIDEFS = read(GUI_ROOT / "src" / "ecinidefs.h")
DLINIDEFS = read(GUI_ROOT / "src" / "dlinidefs.h")
STATE = read(GUI_ROOT / "src" / "ecprojectstate.h")
PROJECT = read(GUI_ROOT / "src" / "ecproject.cpp")
DLPROJECT = read(GUI_ROOT / "src" / "dlproject.cpp")
DESC = read(GUI_ROOT / "src" / "variable_desc.cpp")
MODEL_H = read(GUI_ROOT / "src" / "variable_model.h")

engine_available = ENGINE_TAGS.is_file() and ENGINE_READER.is_file()

#: project key -> (GUI constant, engine SNTags slot)
PROJECT_KEYS = {
    "spectro_meth": ("INI_SCREEN_SETTINGS_105", 58),
    "spectro_water": ("INI_SCREEN_SETTINGS_106", 59),
}

#: metadata key -> GUI constant
META_KEYS = {
    "spectro_a": "INI_VARDESC_SPECTRO_A",
    "spectro_b": "INI_VARDESC_SPECTRO_B",
}


class TheProjectSettingsAgree(unittest.TestCase):

    def test_the_gui_declares_and_writes_and_reads_each(self):
        for key, (const, _) in PROJECT_KEYS.items():
            self.assertRegex(
                INIDEFS,
                r'const auto %s\s*=\s*QStringLiteral\("%s"\)' % (const, key))
            self.assertIn("setValue(EcIni::%s" % const, PROJECT,
                          "%s is declared but never written" % const)
            self.assertIn("value(EcIni::%s" % const, PROJECT,
                          "%s is written but never read back" % const)

    def test_both_default_to_off_in_the_interface(self):
        for field in PROJECT_KEYS:
            self.assertRegex(
                STATE, r"\bint %s = 0;" % field,
                "%s no longer defaults to off in the interface" % field)

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_declares_each_at_the_expected_slot(self):
        tags = read(ENGINE_TAGS)
        for key, (_, slot) in PROJECT_KEYS.items():
            self.assertRegex(
                tags, r"SNTags\(%d\)%%Label\s*/\s*'%s'\s*/" % (slot, key),
                "the engine does not have %r at SNTags(%d)" % (key, slot))

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_defaults_to_off_too(self):
        reader = read(ENGINE_READER)
        self.assertRegex(reader, r"RPSetup%spectro_meth\s*=\s*'none'")
        self.assertRegex(reader, r"RPSetup%spectro_water\s*=\s*\.false\.")


class TheColumnCoefficientsAgree(unittest.TestCase):

    def test_the_gui_declares_and_writes_and_reads_each(self):
        for key, const in META_KEYS.items():
            self.assertRegex(
                DLINIDEFS,
                r'const auto %s\s*=\s*QStringLiteral\("%s"\)' % (const, key))
            self.assertIn("DlIni::%s" % const, DLPROJECT)
        self.assertIn("var.spectroA()", DLPROJECT)
        self.assertIn("var.setSpectroA(", DLPROJECT)

    def test_they_default_to_zero_when_the_file_does_not_say(self):
        #> Zero is the identity. A metadata file written before these keys
        #> existed declares, correctly, that nothing is to be removed.
        for const in META_KEYS.values():
            self.assertRegex(
                DLPROJECT,
                r"DlIni::%s, 0\.0\)" % const,
                "%s does not default to zero on read" % const)
        self.assertRegex(DESC, r"spectroA_\(0\.0\)")
        self.assertRegex(DESC, r"spectroB_\(0\.0\)")

    def test_the_names_do_not_collide_with_the_linear_calibration(self):
        for spectro, calib in (("spectro_a", "a_value"),
                               ("spectro_b", "b_value")):
            self.assertNotIn(calib, spectro)
            self.assertNotIn(spectro, calib)

    def test_the_table_rows_were_appended_not_inserted(self):
        #> The enum is the row order of a transposed table. Anything inserted
        #> above renumbers every row beneath it, and the delegate, the flags
        #> and the header labels are all keyed on those numbers.
        order = re.search(r"enum VarItem\s*\{(.*?)\}", MODEL_H, re.S)
        self.assertIsNotNone(order)
        #> Comments first: they contain commas, and splitting on those would
        #> shred them into things that look like enumerators.
        body = "\n".join(l for l in order.group(1).splitlines()
                         if not l.strip().startswith("//"))
        names = [n.strip() for n in body.split(",") if n.strip()]
        self.assertEqual(
            names[-3:], ["SPECTROA", "SPECTROB", "VARNUMCOLS"],
            "the two coefficient rows are not the last two before "
            "VARNUMCOLS: %s" % names[-4:])

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_declares_the_same_metadata_keys(self):
        tags = read(ENGINE_META_TAGS)
        for key in META_KEYS:
            self.assertRegex(
                tags, r"ANTags\(\d+\)%%Label / 'col_1_%s'" % key,
                "the engine's metadata table has no col_1_%s" % key)

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_defaults_them_to_zero(self):
        meta = read(ENGINE_META_READ)
        self.assertRegex(meta, r"LocCol\(i\)%spectro_a = 0d0")
        self.assertRegex(meta, r"LocCol\(i\)%spectro_b = 0d0")


class TheWaterChannelIsLabelled(unittest.TestCase):
    """It is offered, and it says what it is."""

    def setUp(self):
        self.page = read(GUI_ROOT / "src" / "advprocessingoptions.cpp")

    def test_the_water_switch_exists_and_is_separate(self):
        self.assertIn("spectroWaterCheckBox", self.page)
        self.assertIn("setScreenSpectroWater", self.page)

    def test_its_tooltip_declares_the_form_unpublished(self):
        #> The user asked for this specifically: the water branch is EddyUH's
        #> and its own source says the derivation is not published. Anyone
        #> switching it on has to be told so at the point of switching.
        m = re.search(r"spectroWaterCheckBox->setToolTip\(tr\((.*?)\)\);",
                      self.page, re.S)
        self.assertIsNotNone(m, "the water checkbox has no tooltip")
        tip = m.group(1)
        self.assertIn("not published", tip)
        self.assertIn("EddyUH", tip)
        self.assertIn("not part of the published", tip)

    def test_it_is_greyed_without_the_correction_itself(self):
        self.assertIn("spectroWaterCheckBox->setEnabled(", self.page)


class TheCoefficientConventionIsWarnedAbout(unittest.TestCase):
    """EddyUH's published coefficients are not interchangeable with these.

    Both divide by the same polynomial, but EddyUH folds the dilution in - its
    a = -1 is pure dilution - while EddyFlow corrects the density separately
    and its identity is zero. Someone reading the comparison document, or a
    Rella (2010) table, will have a number in hand and a box to type it into,
    and nothing about the two formulae shows the difference. The warning has
    to be at the point of entry.
    """

    def setUp(self):
        self.page = read(GUI_ROOT / "src" / "advprocessingoptions.cpp")
        self.table = read(GUI_ROOT / "src" / "variable_tableview.cpp")

    def test_the_processing_page_warns(self):
        m = re.search(r"spectroCheckBox->setToolTip\(tr\((.*?)\)\);",
                      self.page, re.S)
        self.assertIsNotNone(m, "the spectroscopic checkbox has no tooltip")
        tip = m.group(1)
        self.assertIn("spectroscopic only", tip.lower())
        self.assertIn("add one", tip.lower())
        self.assertIn("twice", tip.lower())

    def test_the_column_header_warns_too(self):
        #> The table is where the number is actually typed.
        m = re.search(r'addSection\(tr\("Spectroscopic <i>a</i>"\), tr\((.*?)\)\);',
                      self.table, re.S)
        self.assertIsNotNone(m, "the Spectroscopic a column has no tooltip")
        tip = m.group(1)
        self.assertIn("Add one", tip)
        self.assertIn("EddyUH", tip)


if __name__ == "__main__":
    unittest.main()
