"""The assessment-file validator, exercised against real files.

`AncillaryFileTest` had no test of any kind. It is not unit-testable from here
without a Qt harness - it wants an `EcProject` and a `QTextBrowser` - so this
reimplements the two rules it now runs on and asserts they classify real files
correctly. That is a narrow guarantee, but it is the one that matters: the
validator's job is to agree with the engine about what this file is, and the
engine's reader uses exactly these two rules.

The files under test are the ones the engine actually writes, taken from the
engine's regression fixtures, plus the sample the interface ships. The
multi-hygrometer case is the one the old row arithmetic got wrong: it counted
14 rows per non-water gas and knew nothing about the 11-row RH block a second
hygrometer adds, so every section after the blocks was located 11 rows early.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
FIXTURES = ENGINE_ROOT / "tests" / "regression"
TEMPLATE = (GUI_ROOT / "file-templates"
            / "eddyflow_sample_spectral_assessment.txt")

RH_ROWS = 9
MONTH_ROWS = 12
MONTHS = ("January February March April May June July August September "
          "October November December").split()


def blocks_of(lines):
    """Every TFP block, by the validator's two rules."""
    found = []
    for i, line in enumerate(lines):
        words = line.split()
        if "TFP" not in words or len(words) < 3:
            continue
        name = " ".join(words[:words.index("TFP")])
        rh = "numerosity" in words
        found.append({"header": i, "name": name, "rh": rh,
                      "rows": RH_ROWS if rh else MONTH_ROWS})
    return found


def row_starting(lines, word):
    for i, line in enumerate(lines):
        if line.split() and line.split()[0].startswith(word):
            return i
    return -1


def structure_ok(lines):
    """What testSpectraF asserts, restated."""
    if not lines or not lines[0].startswith(
            "Transfer_function_parameters_(TFP)"):
        return False, "preamble"
    found = blocks_of(lines)
    if not found:
        return False, "no blocks"
    if not found[0]["rh"]:
        return False, "first block is not the water RH table"
    for block in found:
        if block["header"] + block["rows"] >= len(lines):
            return False, "block %s truncated" % block["name"]
        for k in range(block["rows"]):
            words = lines[block["header"] + 1 + k].split()
            if block["rh"]:
                if words[:2] != ["RH", "class"]:
                    return False, "%s row %d" % (block["name"], k)
            elif words[:1] != [MONTHS[k]]:
                return False, "%s row %d" % (block["name"], k)
    exp = row_starting(lines, "RH/fc_exponential_fit_parameters")
    hp = row_starting(lines, "High-pass_correction_factor_model_parameters")
    if exp < 0:
        return False, "no exponential-fit section"
    if hp < 0:
        return False, "no high-pass section"
    if hp <= exp:
        return False, "sections out of order"
    return True, ""


@unittest.skipUnless(FIXTURES.exists(), "engine checkout not beside this one")
class RealFilesValidate(unittest.TestCase):

    def _lines(self, path):
        return path.read_text(encoding="utf-8",
                              errors="replace").splitlines()

    def test_the_shipped_sample_validates(self):
        """It is what the interface offers as an example of a good file, and it
        had drifted: a preamble line the engine stopped writing, a separator two
        dashes short, and a `none` block for the retired fourth slot."""
        ok, why = structure_ok(self._lines(TEMPLATE))
        self.assertTrue(ok, "shipped sample fails its own validator: %s" % why)

    def test_a_single_hygrometer_file_validates(self):
        ok, why = structure_ok(self._lines(FIXTURES / "sa_n_gas_2grp.txt"))
        self.assertTrue(ok, why)

    def test_a_two_hygrometer_file_validates(self):
        """The case the row arithmetic could not express. `sa_n_gas_fitted.txt`
        carries an `H2O_2` RH block after the gas blocks."""
        lines = self._lines(FIXTURES / "sa_n_gas_fitted.txt")
        found = blocks_of(lines)
        rh = [b for b in found if b["rh"]]
        self.assertEqual(2, len(rh),
                         "this fixture is the two-hygrometer case; if it has "
                         "one RH block the test is no longer testing that")
        ok, why = structure_ok(lines)
        self.assertTrue(ok, why)

    def test_trailing_tokens_do_not_change_a_block_name(self):
        """`groups=`, `var=`, `instr=` and `exp=` all sit past the columns."""
        lines = self._lines(FIXTURES / "sa_n_gas_fitted.txt")
        names = {b["name"] for b in blocks_of(lines)}
        self.assertIn("CO2_1", names)
        self.assertIn("CO2_2", names)
        self.assertIn("H2O_2", names)
        for name in names:
            self.assertNotIn("=", name,
                             "a trailing token leaked into the block name")

    def test_the_tail_is_not_at_a_fixed_offset(self):
        """The whole point: the two files put their tail in different places,
        so anything that finds it by counting is wrong for one of them."""
        one = self._lines(FIXTURES / "sa_n_gas_2grp.txt")
        two = self._lines(FIXTURES / "sa_n_gas_fitted.txt")
        self.assertNotEqual(
            row_starting(one, "RH/fc_exponential_fit_parameters"),
            row_starting(two, "RH/fc_exponential_fit_parameters"),
            "if these agreed, counting would have worked and this check "
            "would prove nothing")


if __name__ == "__main__":
    unittest.main()
