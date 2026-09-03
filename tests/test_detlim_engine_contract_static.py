"""The detection limit keys reach the engine, and the two sides agree.

Modelled on test_pwb_engine_contract_static.py, and for the same reason: the
engine writes values into the project file while the interface applies its own
defaults on load and saves them, so a disagreement means the same project
computes different fluxes depending on whether it was opened here first.

Three specific ways this feature could break silently:

**A key the engine never sees.** The interface writes into
``[RawProcess_Settings]``, which the engine sweeps with the prefix
``RawProcess``. A key spelled differently on the two sides is simply not
found, the engine keeps its literal default, and the setting does nothing.

**A default that drifts.** The whole point of the offset and width settings
is that they are Wienhold's, so that switching the feature on reproduces the
published method. If one side moves, enabling the feature quietly stops doing
that.

**A default that stops being off.** ``detlim_meth`` must default to 0 on both
sides or an existing project starts computing a column it never asked for.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_TAGS = ENGINE_ROOT / "src" / "src_rp" / "m_rp_global_var.f90"
ENGINE_READER = ENGINE_ROOT / "src" / "src_rp" / "read_ini_rp.f90"

INIDEFS = GUI_ROOT / "src" / "ecinidefs.h"
STATE = GUI_ROOT / "src" / "ecprojectstate.h"
PROJECT = GUI_ROOT / "src" / "ecproject.cpp"
DIALOG = GUI_ROOT / "src" / "detlimsettingsdialog.cpp"

#: key name -> (GUI constant, engine state field)
KEYS = {
    "detlim_meth": ("INI_SCREEN_SETTINGS_102", "detlim_meth"),
    "detlim_offset_s": ("INI_SCREEN_SETTINGS_103", "detlim_offset_s"),
    "detlim_window_s": ("INI_SCREEN_SETTINGS_104", "detlim_window_s"),
}

#: Wienhold's geometry, and off. Both sides must carry these.
SHARED_DEFAULTS = {
    "detlim_offset_s": ("RPSetup%detlim_offset_s = ", 100.0),
    "detlim_window_s": ("RPSetup%detlim_window_s = ", 50.0),
}


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


engine_available = ENGINE_TAGS.is_file() and ENGINE_READER.is_file()


@unittest.skipUnless(engine_available,
                     "engine repo not checked out beside the GUI")
class TheDetlimKeysReachTheEngine(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        tags = read(ENGINE_TAGS)
        cls.tags = {}
        for line in tags.splitlines():
            if line.lstrip().startswith("!"):
                continue
            m = re.search(r"SNTags\((\d+)\)%Label\s*/\s*'([^']*)'", line)
            if m and m.group(2):
                cls.tags[m.group(2)] = int(m.group(1))
        cls.reader = read(ENGINE_READER)
        cls.inidefs = read(INIDEFS)
        cls.project = read(PROJECT)

    def test_the_table_was_found_at_all(self):
        #> Guards against a vacuous parse making everything below pass.
        self.assertGreater(len(self.tags), 100)

    def test_the_engine_declares_every_key(self):
        for key in KEYS:
            self.assertIn(
                key, self.tags,
                "the engine's SNTags table has no label %r, so a project "
                "writing it would be ignored" % key,
            )

    def test_the_gui_declares_every_key_under_the_expected_constant(self):
        for key, (const, _) in KEYS.items():
            self.assertRegex(
                self.inidefs,
                r'const auto %s\s*=\s*QStringLiteral\("%s"\)' % (const, key),
                "%s is not %r in ecinidefs.h" % (const, key),
            )

    def test_the_gui_writes_every_key(self):
        for key, (const, _) in KEYS.items():
            self.assertIn(
                "setValue(EcIni::%s" % const,
                self.project,
                "the interface declares %s but never writes it, so the "
                "setting would never reach the engine" % const,
            )

    def test_the_gui_reads_every_key_back(self):
        for key, (const, _) in KEYS.items():
            self.assertIn(
                "value(EcIni::%s" % const,
                self.project,
                "the interface writes %s but never reads it, so the setting "
                "would reset every time a project is opened" % const,
            )

    def test_the_engine_reads_each_key_under_its_found_guard(self):
        for key in KEYS:
            index = self.tags[key]
            self.assertIn(
                "SNTagFound(%d)" % index,
                self.reader,
                "%s sits at SNTags(%d) but is never read under its found "
                "guard" % (key, index),
            )


@unittest.skipUnless(engine_available,
                     "engine repo not checked out beside the GUI")
class TheDetlimDefaultsAgree(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.state = read(STATE)
        cls.reader = read(ENGINE_READER)
        cls.dialog = read(DIALOG)

    def gui_default(self, field):
        m = re.search(r"\b%s\s*=\s*([0-9.]+)\s*;" % field, self.state)
        self.assertIsNotNone(
            m, "ecprojectstate.h states no default for %s" % field)
        return float(m.group(1))

    def engine_default(self, anchor):
        for line in self.reader.splitlines():
            if anchor in line and not line.lstrip().startswith("!"):
                value = line.split(anchor, 1)[1].strip()
                return float(value.replace("d0", "").rstrip("."))
        self.fail("read_ini_rp.f90 states no default for %r" % anchor)

    def test_the_shared_defaults_match(self):
        for field, (anchor, expected) in SHARED_DEFAULTS.items():
            here = self.gui_default(field)
            there = self.engine_default(anchor)
            self.assertAlmostEqual(
                here, there, places=6,
                msg="%s: %s here, %s in read_ini_rp.f90" % (field, here, there))
            self.assertAlmostEqual(
                here, expected, places=6,
                msg="%s moved on both sides at once - it is no longer "
                    "Wienhold's value" % field)

    def test_the_feature_is_off_on_both_sides(self):
        self.assertAlmostEqual(self.gui_default("detlim_meth"), 0.0)
        self.assertRegex(
            self.reader,
            r"RPSetup%detlim_meth\s*=\s*'none'",
            "the engine no longer defaults the method to none",
        )

    def test_the_dialog_offers_wienholds_values_as_its_reset(self):
        #> Restore Default Values in the dialog must land on the same numbers,
        #> or resetting produces a project the engine would sanitise.
        self.assertRegex(self.dialog, r"DefaultOffset\s*=\s*100\.0")
        self.assertRegex(self.dialog, r"DefaultWindow\s*=\s*50\.0")
        self.assertRegex(self.dialog, r"DefaultMethod\s*=\s*0")

    def test_the_dialog_mirrors_the_engines_rejection_rule(self):
        #> The engine refuses a window at or above its own offset. The dialog
        #> warns on the same condition; a looser test here would let a user
        #> save a combination that silently falls back.
        self.assertIn(
            "windowSpin->value() >= offsetSpin->value()", self.dialog,
            "the dialog's warning no longer matches the engine's rule",
        )
        self.assertIn(
            "RPSetup%detlim_window_s >= RPSetup%detlim_offset_s", self.reader,
            "the engine's rejection rule changed; the dialog's warning has "
            "to change with it",
        )


if __name__ == "__main__":
    unittest.main()
