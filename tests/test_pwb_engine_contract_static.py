"""The PWB keys this interface writes are the ones the engine reads.

`pwb_detect_on_raw` is why this file exists (that setting has since been
retired altogether, but the class of mistake it made has not). The interface wrote it, the
engine declared `pwb_detect_prewpl`, and `SearchLocalTags` matches tag labels
by **exact equality** - so the two never met. The checkbox had no effect on any
run, the pre-processing pass was off for every project ever saved, and nothing
anywhere said so: both sides compiled, both sides ran, and the only symptom was
a setting that silently did nothing.

Nothing at build time connects the two repositories, so the only thing that can
catch that class of mistake is a check that reads both. This is it, for the PWB
group: every key the interface writes must appear in the engine's tag table,
and the shared defaults must agree.

The defaults matter as much as the names. This interface writes every PWB key
unconditionally, so its default is what lands in the file, and the engine's
default is what a file *without* the key gets. When the two disagree, a project
made here and a project imported by the engine's EddyPro converter behave
differently while both look correct - which is exactly what `pwb_smoothing_width`
did, at 6 here against 5 there.

Skipped when the engine is not checked out beside this repository, like the
other cross-repo checks in this suite.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"

ENGINE_TAGS = ENGINE_ROOT / "src" / "src_rp" / "m_rp_global_var.f90"
ENGINE_READER = ENGINE_ROOT / "src" / "src_rp" / "read_ini_rp.f90"

INI_DEFS = "src/ecinidefs.h"
STATE = "src/ecprojectstate.h"
PROJECT = "src/ecproject.cpp"

#: The interface field -> the engine assignment that sets the same setting when
#: the key is absent. Both sides are read out of source; this table only says
#: which pairs with which.
SHARED_DEFAULTS = {
    "n_bootstrap": ("PWBSetup%n_bootstrap = ", "99"),
    "block_length_s": ("PWBSetup%block_length_s = ", "20"),
    "min_valid_frac": ("PWBSetup%min_valid_frac = ", "0.3"),
    "hdi_thresh_s": ("PWBSetup%hdi_thresh_s = ", "0.5"),
    "dev_thresh_s": ("PWBSetup%dev_thresh_s = ", "0.5"),
    "hdi_prefilter_s": ("PWBSetup%hdi_prefilter_s = ", "1.0"),
    "smoothing_width": ("PWBSetup%smoothing_width = ", "5"),
    "random_seed": ("PWBSetup%random_seed = ", "2024"),
}


def read(path):
    return Path(path).read_text(encoding="utf-8", errors="replace")


def gui(rel):
    return read(GUI_ROOT / rel)


def engine_numeric_tags():
    """label -> index, for the RP numeric tag table."""
    out = {}
    for line in read(ENGINE_TAGS).splitlines():
        if line.lstrip().startswith("!"):
            continue
        m = re.search(r"SNTags\((\d+)\)%Label\s*/\s*'([^']*)'", line)
        if m:
            out[m.group(2)] = int(m.group(1))
    return out


def gui_pwb_keys():
    """Every PWB name declared, whether written, read or retired."""
    defs = gui(INI_DEFS)
    out = {}
    for m in re.finditer(
            r'const auto (INI_PWB_TIMELAG_\w+)\s*=\s*QStringLiteral\("([^"]*)"\)',
            defs):
        out[m.group(1)] = m.group(2)
    return out


def gui_written_constants():
    """The constants saveEcProject actually writes into the file.

    Declared-but-not-written is a real category here: INI_PWB_TIMELAG_0..7 are
    the flat per-species windows (pwb_co2_min_lag and its seven siblings),
    which are still read so an older project keeps its settings, but are never
    written back - the same read-without-write retirement shape as col_co2.
    The engine retired them too, so they are absent from its tag table, and a
    check over everything declared would fail on them for the wrong reason.
    """
    return set(re.findall(r"setValue\(EcIni::(INI_PWB_TIMELAG_\w+)",
                          gui(PROJECT)))


def engine_default(assignment):
    """The literal the engine assigns, with Fortran's d-exponent stripped."""
    for line in read(ENGINE_READER).splitlines():
        if line.lstrip().startswith("!"):
            continue
        if assignment in line:
            value = line.split(assignment, 1)[1].strip()
            return value.replace("d0", "").rstrip(".") or "0"
    return None


@unittest.skipUnless(ENGINE_TAGS.exists(),
                     "eddyflow-engine not checked out beside this repository")
class ThePwbKeysReachTheEngine(unittest.TestCase):

    def setUp(self):
        self.tags = engine_numeric_tags()
        self.keys = gui_pwb_keys()

    def test_the_table_was_found_at_all(self):
        #> Guard the guard: a parse that silently returns nothing would make
        #> every assertion below vacuously true.
        self.assertGreater(len(self.tags), 100)
        self.assertGreater(len(self.keys), 10)

    def test_every_flat_key_written_here_is_one_the_engine_declares(self):
        #> The invariant that `pwb_detect_on_raw` broke, stated as narrowly as
        #> it is true: what this interface WRITES has to be something the
        #> engine reads. The per-gas windows are built inline as
        #> gas_<i>_pwb_*, not from this table, and the engine's own suite
        #> covers those.
        written = gui_written_constants()
        #> Eight live settings: INI_PWB_TIMELAG_8..15. The bound only guards
        #> against a parse that finds nothing and makes the loop below
        #> vacuous; it is not a count anyone should have to maintain upward.
        self.assertGreaterEqual(len(written), 8, "no PWB writes found")
        for name in sorted(written):
            key = self.keys.get(name)
            self.assertIsNotNone(key, "%s is written but not declared" % name)
            self.assertIn(key, self.tags,
                          "%s writes %r, which no SNTags label matches - the "
                          "engine will never see it" % (name, key))

    def test_the_read_only_legacy_windows_stay_read_only(self):
        #> INI_PWB_TIMELAG_0..7 are the flat per-species windows. They are
        #> retired on both sides: read here so an older project keeps what it
        #> said, absent from the engine's table entirely. Writing one again
        #> would put a key in the file that nothing on the engine side reads -
        #> which is the whole failure this file guards.
        written = gui_written_constants()
        for idx in range(8):
            name = "INI_PWB_TIMELAG_%d" % idx
            self.assertIn(name, self.keys)
            self.assertNotIn(name, written,
                             "%s is retired; writing it again puts a dead key "
                             "in the file" % name)
            self.assertNotIn(self.keys[name], self.tags)

    def test_the_three_retired_settings_are_removed_and_never_read(self):
        #> pwb_detect_prewpl chose between detecting before and after the
        #> pointwise mixing-ratio conversion. Both ran on rotated 20 Hz data,
        #> and that conversion runs before time-lag compensation - so after it
        #> the cell temperature and water signals sit in the gas series at the
        #> wrong relative lag, and the gas series is the one being
        #> cross-correlated. Detection is pre-WPL now, with nothing to choose.
        #>
        #> pwb_approx_ccf and pwb_max_ar_order were speed options worth well
        #> under a percent of runtime apiece, and both cost accuracy for it.
        #>
        #> pwb_detect_on_raw is the spelling the interface used before the
        #> rename; no engine reader ever had it.
        #>
        #> All four are removed on save and none is read back: a key nothing
        #> reads is a key someone will eventually edit expecting it to matter.
        project = gui(PROJECT)
        retired = {
            "INI_PWB_TIMELAG_16_RETIRED": "pwb_approx_ccf",
            "INI_PWB_TIMELAG_17_RETIRED": "pwb_max_ar_order",
            "INI_PWB_TIMELAG_18_RETIRED": "pwb_detect_prewpl",
            "INI_PWB_TIMELAG_18_LEGACY": "pwb_detect_on_raw",
        }
        for name, key in retired.items():
            self.assertEqual(self.keys.get(name), key)
            self.assertNotIn(key, self.tags,
                             "%s is retired but the engine still declares it"
                             % key)
            self.assertIn("project_ini.remove(EcIni::%s)" % name, project)
            self.assertNotIn("value(EcIni::%s" % name, project)


@unittest.skipUnless(ENGINE_READER.exists(),
                     "eddyflow-engine not checked out beside this repository")
class ThePwbDefaultsAgree(unittest.TestCase):

    def setUp(self):
        self.state = gui(STATE)

    def gui_default(self, field):
        m = re.search(r"\b%s\s*=\s*([0-9.]+)\s*;" % re.escape(field), self.state)
        self.assertIsNotNone(m, "no default found for %s" % field)
        return m.group(1)

    def test_the_shared_defaults_match(self):
        #> This interface writes every one of these unconditionally, so its
        #> default is what lands in the file; the engine's is what a file
        #> lacking the key gets. A disagreement means the same project behaves
        #> differently depending on which side wrote it.
        for field, (assignment, expected) in sorted(SHARED_DEFAULTS.items()):
            here = float(self.gui_default(field))
            there = engine_default(assignment)
            self.assertIsNotNone(
                there, "the engine no longer assigns %r" % assignment)
            self.assertAlmostEqual(
                here, float(there), places=6,
                msg="%s: %s here, %s in read_ini_rp.f90" % (field, here, there))
            self.assertAlmostEqual(here, float(expected), places=6,
                                   msg="%s moved on both sides at once" % field)

    def test_no_control_offers_a_detection_stage_or_a_speed_option(self):
        #> The settings are gone from both sides, so the dialog must not still
        #> present them - a checkbox wired to nothing is worse than no
        #> checkbox, because it looks like it works.
        dialog = gui("src/pwbtimelagsettingsdialog.cpp")
        for gone in ("detectOnRawCheckBox", "approxCcfCheckBox",
                     "maxArOrderCheckBox", "maxArOrderSpin", "Speed options"):
            self.assertNotIn(gone, dialog)
        reader = read(ENGINE_READER)
        for gone in ("PWBSetup%detect_prewpl", "PWBSetup%approx_ccf",
                     "PWBSetup%max_ar_order"):
            self.assertNotIn(gone, reader)


if __name__ == "__main__":
    unittest.main()
