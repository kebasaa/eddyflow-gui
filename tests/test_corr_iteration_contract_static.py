"""The iterative correction agrees across the interface and the engine.

Three keys in ``[Project]``, because both applications run the loop. The
interface's job is to make the two numbers mean what the engine reads them to
mean, and to say what the loop is for - a checkbox labelled "iterate" tells
nobody why anything needs iterating.

The one value worth pinning hardest is the **tolerance of zero**. It does not
mean "converge immediately"; it means "run every pass", which is EddyUH's
behaviour, and both sides have to agree or the default silently stops after
two passes.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_TAGS = ENGINE_ROOT / "src" / "src_common" / "m_common_global_var.f90"
ENGINE_DECODER = (ENGINE_ROOT / "src" / "src_common"
                  / "write_processing_project_variables.f90")
ENGINE_RP = ENGINE_ROOT / "src" / "src_rp" / "eddyflow-rp_main.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


INIDEFS = read(GUI_ROOT / "src" / "ecinidefs.h")
STATE = read(GUI_ROOT / "src" / "ecprojectstate.h")
PROJECT = read(GUI_ROOT / "src" / "ecproject.cpp")
PAGE = read(GUI_ROOT / "src" / "advspectraloptions.cpp")

KEYS = {
    "corr_iter_meth": ("INI_PROJECT_84", 8),
    "corr_iter_max": ("INI_PROJECT_85", 9),
    "corr_iter_tol": ("INI_PROJECT_86", 10),
}

engine_available = ENGINE_TAGS.is_file() and ENGINE_DECODER.is_file()


class TheKeysReachTheEngine(unittest.TestCase):

    def test_the_gui_declares_writes_and_reads_each(self):
        for key, (const, _) in KEYS.items():
            self.assertRegex(
                INIDEFS,
                r'const auto %s\s*=\s*QStringLiteral\("%s"\)' % (const, key))
            self.assertIn("setValue(EcIni::%s" % const, PROJECT)
            self.assertIn("EcIni::%s" % const, PROJECT)

    def test_they_are_project_keys_not_rawprocess_ones(self):
        #> Both applications run the loop, and FCC never sweeps the
        #> RawProcess groups.
        head = INIDEFS[:INIDEFS.index('QStringLiteral("corr_iter_meth")')]
        self.assertEqual(re.findall(r"INIGROUP_(\w+)\s*=", head)[-1], "PROJECT")

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_holds_them_at_the_expected_slots(self):
        tags = read(ENGINE_TAGS)
        for key, (_, slot) in KEYS.items():
            self.assertRegex(
                tags, r"EPPrjNTags\(%d\)%%Label\s*/\s*'%s'\s*/" % (slot, key))


class ZeroMeansRunEveryPass(unittest.TestCase):

    def test_the_gui_default_is_off_with_eddyuhs_numbers(self):
        self.assertIn("int corr_iter_meth = 0;", STATE)
        self.assertIn("int corr_iter_max = 4;", STATE)
        self.assertIn("qreal corr_iter_tol = 0.0;", STATE)

    @unittest.skipUnless(engine_available, "engine repo not beside the GUI")
    def test_the_engine_agrees_on_all_three(self):
        src = read(ENGINE_DECODER)
        self.assertIn("EddyFlowProj%corr_iter_meth = .false.", src)
        self.assertIn("EddyFlowProj%corr_iter_max = 4", src)
        self.assertIn("EddyFlowProj%corr_iter_tol = 0d0", src)

    @unittest.skipUnless(ENGINE_RP.is_file(), "engine repo not beside the GUI")
    def test_a_zero_tolerance_cannot_exit_early(self):
        #> The whole reason zero is the default. A `>=` here would make the
        #> default stop after the first comparison.
        self.assertIn("EddyFlowProj%corr_iter_tol > 0d0", read(ENGINE_RP))

    def test_the_spin_says_what_zero_means(self):
        #> A bare 0 in a box labelled "stop below" reads as "stop at once".
        self.assertIn('setSpecialValueText(tr("run every pass"))', PAGE)

    def test_a_negative_tolerance_falls_back_but_zero_survives(self):
        #> Anchored on the READER. The first mention of the key is the
        #> writer, and slicing from there reads the wrong block entirely.
        block = PROJECT[PROJECT.index("const auto t = project_ini.value("):]
        block = block[:block.index(";", block.index("corr_iter_tol = t"))]
        self.assertIn("t >= 0.0", block,
                      "zero must be kept - it is the default and it means "
                      "something")
        self.assertNotIn("t > 0.0", block)

    def test_a_pass_count_below_one_falls_back(self):
        block = PROJECT[PROJECT.index("const auto n = project_ini.value("):]
        block = block[:block.index(";", block.index("corr_iter_max = n"))]
        self.assertIn("n >= 1", block)


class TheControlSaysWhatItIsFor(unittest.TestCase):

    def tip(self):
        m = re.search(r"const QString corrIterTip = tr\((.*?)\);", PAGE, re.S)
        self.assertIsNotNone(m, "the checkbox has no tooltip")
        return "".join(re.findall(r'"([^"]*)"', m.group(1)))

    def test_it_explains_the_circle(self):
        #> "Iterate the correction" says nothing about why anything needs
        #> iterating. The circle is the reason.
        tip = self.tip()
        self.assertIn("evaluated at z/L", tip)
        self.assertIn("corrected sensible heat flux", tip)

    def test_it_says_nothing_compounds(self):
        #> The obvious worry about repeating a correction.
        self.assertIn("nothing compounds", self.tip())

    def test_it_says_where_the_effect_is_and_is_not(self):
        tip = self.tip()
        self.assertIn("strongly non-neutral", tip)
        self.assertIn("hundredths of a percent", tip)

    def test_it_names_the_column_that_reports_convergence(self):
        self.assertIn("corr_iter_dev", self.tip())

    def test_the_tolerance_tooltip_says_it_is_not_eddyuhs(self):
        #> A user reproducing EddyUH must know that setting a tolerance
        #> departs from it.
        m = re.search(r"corrIterTolSpin->setToolTip\(tr\((.*?)\)\);", PAGE, re.S)
        tip = "".join(re.findall(r'"([^"]*)"', m.group(1)))
        self.assertIn("no early exit at all", tip)
        self.assertIn("changes the answer", tip)

    def test_the_numbers_are_greyed_with_the_checkbox(self):
        fn = re.search(
            r"void AdvSpectralOptions::updateCorrIterAvailability\(\)"
            r"\s*\{(.*?)\n\}", PAGE, re.S)
        self.assertIsNotNone(fn)
        body = fn.group(1)
        for w in ("corrIterMaxSpin", "corrIterTolSpin",
                  "corrIterMaxLabel", "corrIterTolLabel"):
            self.assertIn("%s->setEnabled(on)" % w, body)

    def test_the_checkbox_writes_on_the_click_not_the_state(self):
        #> refresh() blocks the project's signals, not the widgets'.
        self.assertIn("connect(corrIterCheckBox, &QCheckBox::clicked", PAGE)
        self.assertNotIn("connect(corrIterCheckBox, &QCheckBox::toggled", PAGE)


if __name__ == "__main__":
    unittest.main()
