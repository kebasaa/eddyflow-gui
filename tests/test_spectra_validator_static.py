"""The assessment-file validator, exercised against real files.

`AncillaryFileTest` is not unit-testable from here without a Qt harness - it
wants an `EcProject` and a `QTextBrowser` - so this restates the rules it runs
and asserts they classify real files correctly: the files the engine writes,
taken from the engine's regression fixtures, held against the templates the
interface ships.

The templates carry one prototype of each kind of block, headed `<GAS>` or
`<gas>`, and every block in a file is compared with the prototype of its kind.
Whether a gas can fail the file depends on the project: only a gas the raw
data holds (a record with a column) must have a block, and only its values
are checked. A gas the project names without a column, or does not name at
all, is read and skipped - which is what the engine does with it.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
FIXTURES = ENGINE_ROOT / "tests" / "regression"
TEMPLATES = GUI_ROOT / "file-templates"
SPECTRA_TEMPLATE = TEMPLATES / "eddyflow_sample_spectral_assessment.txt"
TIMELAG_TEMPLATE = TEMPLATES / "eddyflow_sample_timelag_opt.txt"
TIMELAG_FIXTURE = (FIXTURES / "tlag_after"
                   / "eddyflow_CH-LAE_COS25_timelag_opt_adv.txt")

RH_ROWS = 9
MONTH_ROWS = 12
ERROR = -9999.0


def tokens(text):
    """parseFile: split on single spaces, empty parts dropped."""
    return [line.split() for line in text.splitlines()]


def read(path):
    return tokens(path.read_text(encoding="utf-8", errors="replace"))


def row(lines, i):
    return lines[i] if 0 <= i < len(lines) else []


def number(word):
    try:
        return float(word)
    except (TypeError, ValueError):
        return 0.0


# --- the project ------------------------------------------------------------

class Project:
    """The gas record list: (slug, raw column) in record order."""

    def __init__(self, *records):
        self.records = list(records)

    def measured(self, slot):
        return 0 <= slot < len(self.records) and self.records[slot][1] > 0

    def of_species(self, species):
        return [i for i, (slug, _) in enumerate(self.records)
                if slug.lower() == species.lower()]

    def primary_water(self):
        for i in self.of_species("h2o"):
            if self.measured(i):
                return i
        return -1

    def file_name(self, slot):
        slug = self.records[slot][0]
        same = self.of_species(slug)
        if len(same) <= 1:
            return slug.upper()
        return "%s_%d" % (slug.upper(), same.index(slot) + 1)

    def slot_for(self, name):
        whole = self.of_species(name)
        if whole:
            return whole[0]
        match = re.match(r"^(.+)_(\d+)$", name)
        if not match:
            return -1
        same = self.of_species(match.group(1))
        k = int(match.group(2))
        return same[k - 1] if 1 <= k <= len(same) else -1


# --- the spectral file ------------------------------------------------------

def blocks_of(lines):
    found = []
    for i, words in enumerate(lines):
        if "TFP" not in words or len(words) < 3:
            continue
        rh = "numerosity" in words
        found.append({"header": i, "name": " ".join(words[:words.index("TFP")]),
                      "rh": rh, "rows": RH_ROWS if rh else MONTH_ROWS})
    return found


def row_starting(lines, word):
    for i, words in enumerate(lines):
        if words and words[0].startswith(word):
            return i
    return -1


def is_dash_rule(word):
    return bool(word) and set(word) == {"-"}


def is_number(word):
    try:
        float(word)
        return True
    except ValueError:
        return False


def same_labels(model, actual, name=""):
    expected = []
    for word in model:
        expected.extend(name.split() if word == "<GAS>" else [word])
    actual = [w for w in actual if not (len(w) > 1 and "=" in w)]
    if "=" in expected:
        cut = expected.index("=") + 1
        return actual[:cut] == expected[:cut]
    if expected and all(is_number(w) for w in expected):
        return len(actual) == len(expected)
    if len(actual) != len(expected):
        return False
    return all(a == e or (is_dash_rule(a) and is_dash_rule(e))
               for a, e in zip(actual, expected))


def spectra_template(lines):
    blocks = blocks_of(lines)
    tail = row_starting(lines, "RH/fc_exponential_fit_parameters")
    water = blocks[0]
    parts = {"preamble": lines[:water["header"] + 1 + water["rows"]],
             "tail": lines[tail:]}
    for block in blocks:
        if block["name"] != "<GAS>":
            continue
        kind = "rh" if block["rh"] else "month"
        parts[kind + "_header"] = lines[block["header"]]
        parts[kind + "_rows"] = lines[block["header"] + 1:
                                      block["header"] + 1 + block["rows"]]
    while parts["tail"] and not parts["tail"][-1]:
        parts["tail"].pop()
    return parts


def spectra_format(model, lines, project):
    """testSpectraF: the failed checks, empty when the file passes."""
    failed = []
    for i, template_row in enumerate(model["preamble"]):
        if not same_labels(template_row, row(lines, i)):
            failed.append("preamble row %d" % (i + 1))

    blocks = blocks_of(lines)
    covered = set()
    for block in blocks[1:]:
        kind = "rh" if block["rh"] else "month"
        rows = model[kind + "_rows"]
        ok = (bool(block["name"])
              and block["header"] + len(rows) < len(lines)
              and same_labels(model[kind + "_header"],
                              row(lines, block["header"]), block["name"]))
        for i, template_row in enumerate(rows):
            ok = ok and same_labels(template_row,
                                    row(lines, block["header"] + 1 + i))
        if not ok:
            failed.append("%s labels" % block["name"])
        slot = project.slot_for(block["name"])
        if slot >= 0:
            covered.add(slot)

    water = project.primary_water()
    for i in range(len(project.records)):
        if i != water and project.measured(i) and i not in covered:
            failed.append("%s has no block" % project.file_name(i))

    tail = row_starting(lines, "RH/fc_exponential_fit_parameters")
    end = blocks[-1]["header"] + blocks[-1]["rows"] if blocks else 0
    if tail <= end:
        failed.append("tail position")
    else:
        for i, template_row in enumerate(model["tail"]):
            if not same_labels(template_row, row(lines, tail + i)):
                failed.append("tail row %d" % (tail + i + 1))
    return failed


def spectra_science(lines, project):
    """testSpectraS: (failed checks, skipped gases)."""
    failed, skipped = [], []
    blocks = blocks_of(lines)

    def column(block, k):
        return [number(row(lines, block["header"] + 1 + i)[k:k + 1][0]
                       if len(row(lines, block["header"] + 1 + i)) > k else 0)
                for i in range(block["rows"])]

    def fc_good(d):
        return 0.001 <= d <= 10.0

    def fn_ok(fn, fc):
        return all(0.01 <= n <= 10.0 for n, c in zip(fn, fc) if fc_good(c))

    def rh(block, label):
        fn, fc, num = column(block, 6), column(block, 7), column(block, 8)
        if not any(fc_good(c) for c in fc) or all(c == ERROR for c in fc) \
                or not any(n > 0 for n in num) or not fn_ok(fn, fc):
            failed.append(label)

    def month(block, label):
        fn, fc = column(block, 2), column(block, 3)
        if not all(fc_good(c) for c in fc) or not fn_ok(fn, fc):
            failed.append(label)

    if project.measured(project.primary_water()):
        rh(blocks[0], "H2O")
    else:
        skipped.append("H2O")
    for block in blocks[1:]:
        if not project.measured(project.slot_for(block["name"])):
            skipped.append(block["name"])
        elif block["rh"]:
            rh(block, block["name"])
        else:
            month(block, block["name"])
    return failed, skipped


# --- the time-lag file ------------------------------------------------------

def timelag_label(words):
    label = (words[0] if words else "").strip().lower()
    label = label.replace("-", "_").replace("optimisation", "optimization")
    label = label.replace("mimimum", "minimum")
    return label.rstrip(":")


def timelag_line(words):
    text = " ".join(words).lower().replace("-", "_")
    return text.replace("optimisation", "optimization").rstrip(":")


def same_timelag_label(model, actual, gas=""):
    if not model:
        return not actual
    return timelag_label(actual) == timelag_label(model).replace("<gas>", gas)


def timelag_template(lines):
    gas_row = next(i for i, w in enumerate(lines) if "<gas>" in " ".join(w))
    rh_row = next(i for i, w in enumerate(lines) if timelag_label(w)
                  == "h2o_timelag_determinations_as_a_function_of_relative_humidity")
    return {"header": lines[:gas_row], "block": lines[gas_row:rh_row],
            "rh": lines[rh_row:rh_row + 3]}


def block_gas(model, lines, start):
    label = timelag_label(row(lines, start))
    prefix = "number_of_timelags_used_for_"
    if not label.startswith(prefix) or len(label) == len(prefix):
        return ""
    gas = label[len(prefix):]
    for i, template_row in enumerate(model):
        if start + i >= len(lines) and not template_row:
            continue
        if not same_timelag_label(template_row, row(lines, start + i), gas):
            return ""
    return gas


def timelag_format(model, lines, project):
    """testTimeLagF: (failed checks, gas names)."""
    lines = [w for w in lines if not (w and w[0].lower().startswith("pwb_"))]
    failed = []
    for i, template_row in enumerate(model["header"]):
        if not same_timelag_label(template_row, row(lines, i)):
            failed.append("header row %d" % (i + 1))

    cursor, gases = len(model["header"]), []
    while True:
        gas = block_gas(model["block"], lines, cursor)
        if not gas:
            break
        gases.append(gas)
        cursor += len(model["block"])

    def masked(words):
        return re.sub(r"\d+", "#", timelag_line(words))

    rh = (same_timelag_label(model["rh"][0], row(lines, cursor))
          and masked(model["rh"][1]) == masked(row(lines, cursor + 1))
          and [w.lower() for w in model["rh"][2]]
          == [w.lower() for w in row(lines, cursor + 2)])
    if not rh and any(row(lines, i) for i in range(cursor, len(lines))):
        failed.append("unrecognised rows")

    covered = {project.slot_for(g) for g in gases}
    if rh:
        covered.add(project.primary_water())
    for i in range(len(project.records)):
        if project.measured(i) and i not in covered:
            failed.append("%s has no time lag" % project.file_name(i).lower())
    return failed, gases


# --- the tests --------------------------------------------------------------

#: The project behind `sa_n_gas_*`: two analysers, two water records, and a
#: methane record the engine writes a block for but that has no column.
N_GAS_PROJECT = Project(("co2", 5), ("h2o", 6), ("ch4", -1), ("cos", 7),
                        ("n2o", 8), ("co2", 9), ("n2o", 10), ("h2o", 11))


def replace_block(lines, name, fn, fc):
    """\a lines with every month of block \a name set to (fn, fc)."""
    lines = [list(w) for w in lines]
    block = next(b for b in blocks_of(lines) if b["name"] == name)
    for i in range(block["rows"]):
        words = lines[block["header"] + 1 + i]
        lines[block["header"] + 1 + i] = words[:2] + [fn, fc]
    return lines


def drop_block(lines, name):
    block = next(b for b in blocks_of(lines) if b["name"] == name)
    # the header, its rows, and the blank line after it
    return lines[:block["header"]] + lines[block["header"] + block["rows"] + 2:]


@unittest.skipUnless(FIXTURES.exists(), "engine checkout not beside this one")
class SpectralAssessment(unittest.TestCase):

    def setUp(self):
        self.model = spectra_template(read(SPECTRA_TEMPLATE))

    def test_the_template_has_every_prototype(self):
        for part in ("preamble", "month_header", "month_rows",
                     "rh_header", "rh_rows", "tail"):
            self.assertTrue(self.model.get(part), part)

    def test_engine_files_pass(self):
        #: `2grp` was written for the same site with one hygrometer.
        one_hygrometer = Project(*N_GAS_PROJECT.records[:-1])
        for name, project in (("sa_n_gas_fitted.txt", N_GAS_PROJECT),
                              ("sa_n_gas_2grp.txt", one_hygrometer)):
            with self.subTest(name):
                lines = read(FIXTURES / name)
                self.assertEqual([], spectra_format(self.model, lines,
                                                    project))
                failed, skipped = spectra_science(lines, project)
                self.assertEqual([], failed)
                self.assertEqual(["CH4"], skipped)

    def test_the_second_hygrometer_is_checked(self):
        """`H2O_2` is a named nine-row block, held against the RH prototype
        and given the humidity-class value checks."""
        lines = read(FIXTURES / "sa_n_gas_fitted.txt")
        self.assertIn("H2O_2", [b["name"] for b in blocks_of(lines)])
        broken = replace_block(lines, "H2O_2", "-9999.0", "-9999.0")
        self.assertEqual([], spectra_science(lines, N_GAS_PROJECT)[0])
        self.assertIn("H2O_2", spectra_science(broken, N_GAS_PROJECT)[0])

    def test_a_measured_gas_without_a_block_fails(self):
        lines = drop_block(read(FIXTURES / "sa_n_gas_fitted.txt"), "COS")
        self.assertIn("COS has no block",
                      spectra_format(self.model, lines, N_GAS_PROJECT))

    def test_an_unmeasured_gas_without_a_block_passes(self):
        """The file for a site that does not measure methane need not carry
        its block."""
        lines = drop_block(read(FIXTURES / "sa_n_gas_fitted.txt"), "CH4")
        self.assertEqual([], spectra_format(self.model, lines, N_GAS_PROJECT))

    def test_a_measured_gas_without_a_fit_fails(self):
        lines = replace_block(read(FIXTURES / "sa_n_gas_fitted.txt"),
                              "COS", "-9999.00000", "-9999.00000")
        self.assertIn("COS", spectra_science(lines, N_GAS_PROJECT)[0])

    def test_an_unmeasured_gas_without_a_fit_passes(self):
        lines = replace_block(read(FIXTURES / "sa_n_gas_fitted.txt"),
                              "CH4", "-9999.00000", "-9999.00000")
        failed, skipped = spectra_science(lines, N_GAS_PROJECT)
        self.assertEqual([], failed)
        self.assertIn("CH4", skipped)

    def test_blocks_are_matched_by_name_not_position(self):
        """The old test read the second monthly block as methane. Here it is
        CH4 regardless, and a fit missing from N2O_2 - the sixth block - is
        caught."""
        lines = replace_block(read(FIXTURES / "sa_n_gas_fitted.txt"),
                              "N2O_2", "-9999.00000", "-9999.00000")
        self.assertEqual(["N2O_2"], spectra_science(lines, N_GAS_PROJECT)[0])

    def test_a_mislabelled_month_fails(self):
        lines = read(FIXTURES / "sa_n_gas_fitted.txt")
        block = next(b for b in blocks_of(lines) if b["name"] == "COS")
        lines[block["header"] + 1][0] = "Jan"
        self.assertIn("COS labels",
                      spectra_format(self.model, lines, N_GAS_PROJECT))

    def test_the_shipped_template_is_not_a_usable_file(self):
        """It is a layout, not an assessment: every measured gas lacks its
        block."""
        failed = spectra_format(self.model, read(SPECTRA_TEMPLATE),
                                N_GAS_PROJECT)
        self.assertIn("COS has no block", failed)


@unittest.skipUnless(TIMELAG_FIXTURE.exists(),
                     "engine checkout not beside this one")
class TimelagOptimisation(unittest.TestCase):

    #: The project behind the COS25 run: `4th_gas` is a block the engine
    #: wrote for a slot this project does not have.
    PROJECT = Project(("co2", 5), ("h2o", 6), ("n2o", 7), ("co2", 8),
                      ("n2o", 9))

    def setUp(self):
        self.model = timelag_template(read(TIMELAG_TEMPLATE))
        self.lines = read(TIMELAG_FIXTURE)

    def test_an_engine_file_passes(self):
        """Including its `< 30` note, which the engine has since changed to
        `< 15`: the sentence is the format, the number a setting."""
        failed, gases = timelag_format(self.model, self.lines, self.PROJECT)
        self.assertEqual([], failed)
        self.assertEqual(["co2", "4th_gas", "n2o_1", "co2_2", "n2o_2"], gases)

    def test_pwb_provenance_rows_are_not_part_of_the_layout(self):
        lines = [list(w) for w in self.lines]
        lines.insert(1, ["PWB_aggregate_summary:", "true"])
        lines.insert(7, ["PWB_summary_source_for_co2:", "native"])
        self.assertEqual([], timelag_format(self.model, lines,
                                            self.PROJECT)[0])

    def test_a_measured_gas_without_a_block_fails(self):
        project = Project(*self.PROJECT.records, ("cos", 10))
        self.assertIn("cos has no time lag",
                      timelag_format(self.model, self.lines, project)[0])

    def test_water_is_covered_by_the_rh_table(self):
        lines = self.lines[:row_starting(self.lines, "H2O_timelag")]
        self.assertIn("h2o has no time lag",
                      timelag_format(self.model, lines, self.PROJECT)[0])


if __name__ == "__main__":
    unittest.main()
