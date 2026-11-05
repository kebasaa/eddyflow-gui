"""The CEC Settings table is one row per flux; the project file is pairings.

A user arrives with "split my carbonyl sulfide", not with "pair CO2 channel 1
with H2O channel 2 and carry record 4 along". So the table shows one row per
flux - the flux, the water whose moist updrafts are sorted, the gas whose sign
sorts them - and the pairings the engine runs are derived from the rows: rows
that share an octant gas and the water actually read are one pairing.

Nothing new is stored. The rows are rebuilt from the pairings after every edit,
so what the table shows is exactly what the file reloads, and the existing
remapping of pairings keeps covering every rearrangement of the gas records.

Two things are new to the engine contract and are experimental: the octant gas
may be carbonyl sulfide, and a pairing may report only its extras
(cec_<i>_meth = 4). Both moved the project format to 5.2.0.

The grouping is ported below and exercised, and the C++ is pinned to the lines
that port mirrors.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT.parent / "eddyflow-engine"


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


# ------------------------------------------------------------ reference ----
#
# A gas is (slug, instrument, raw column). Indices are 1-based, as in the
# project file. A row is (flux, water, octant); a pairing is
# (meth, octant, water, [extras]).

MAX_PAIRS = 8
MAX_EXTRAS = 4


def real(instrument):
    return bool(instrument) and instrument not in ("none", "other")


def first_of(gases, slug, instrument=None):
    for i, (s, instr, col) in enumerate(gases, start=1):
        if col <= 0 or s != slug:
            continue
        if instrument is not None and instr != instrument:
            continue
        return i
    return 0


def resolved_water(gases, octant, water):
    if water > 0:
        return water
    if octant < 1 or octant > len(gases) or not real(gases[octant - 1][1]):
        return 0
    return first_of(gases, "h2o", gases[octant - 1][1])


def group_rows(gases, rows):
    groups = []
    repeated = False
    for flux, water_cell, octant in rows:
        water = resolved_water(gases, octant, water_cell)
        group = next((g for g in groups
                      if g["octant"] == octant and g["water"] == water), None)
        if group is None:
            group = dict(octant=octant, water=water, cell=water_cell,
                         own=False, wet=False, extras=[])
            groups.append(group)
        if octant > 0 and flux == octant:
            repeated |= group["own"]
            group["own"] = True
        elif flux == 0 or (water > 0 and flux == water):
            repeated |= group["wet"]
            group["wet"] = True
        elif flux in group["extras"]:
            repeated = True
        else:
            group["extras"].append(flux)
    pairs = []
    for g in groups:
        meth = (1 if g["own"] and g["wet"] else 2 if g["wet"]
                else 3 if g["own"] else 4)
        pairs.append((meth, g["octant"], g["cell"], g["extras"]))
    return pairs, repeated


def rows_from_pairs(gases, pairs):
    rows = []
    for meth, octant, water, extras in pairs:
        if meth == 0:
            continue
        if meth in (1, 3):
            rows.append((octant, water, octant))
        if meth in (1, 2):
            rows.append((resolved_water(gases, octant, water), water, octant))
        rows.extend((extra, water, octant) for extra in extras)
    return rows


def rows_fit(gases, rows):
    pairs, repeated = group_rows(gases, rows)
    if repeated or len(pairs) > MAX_PAIRS:
        return False
    return all(len(p[3]) <= MAX_EXTRAS for p in pairs)


#: base_cec_cos's layout: a MIRO with CO2, H2O, COS and N2O, a slot with no
#: column, and an LI-7200 with CO2 and H2O.
SITE = [("co2", "miro", 7), ("h2o", "miro", 8), ("ch4", "none", 0),
        ("cos", "miro", 10), ("n2o", "miro", 9), ("co2", "li7200", 14),
        ("h2o", "li7200", 15)]


class TheRowsAreAViewOfThePairings(unittest.TestCase):
    def test_a_stated_project_round_trips(self):
        pairs = [(1, 1, 2, [4, 5]), (1, 6, 7, [])]
        rows = rows_from_pairs(SITE, pairs)
        self.assertEqual(rows, [(1, 2, 1), (2, 2, 1), (4, 2, 1), (5, 2, 1),
                                (6, 7, 6), (7, 7, 6)])
        self.assertEqual(group_rows(SITE, rows)[0], pairs)

    def test_same_analyser_and_that_analysers_water_are_one_pairing(self):
        #> Otherwise the engine would read the same two channels twice and
        #> drop the second pairing, with its flux in it.
        rows = [(1, 2, 1), (4, 0, 1)]
        pairs, _ = group_rows(SITE, rows)
        self.assertEqual(len(pairs), 1)
        self.assertEqual(pairs[0][3], [4])

    def test_cos_octants_are_a_pairing_of_their_own(self):
        rows = [(1, 2, 1), (2, 2, 1), (4, 2, 4)]
        pairs, _ = group_rows(SITE, rows)
        self.assertEqual(pairs, [(1, 1, 2, []), (3, 4, 2, [])])

    def test_a_pairing_kept_only_for_its_extras_is_meth_four(self):
        pairs, _ = group_rows(SITE, [(5, 2, 4)])
        self.assertEqual(pairs, [(4, 4, 2, [5])])
        self.assertEqual(rows_from_pairs(SITE, pairs), [(5, 2, 4)])

    def test_a_pairing_that_is_off_shows_no_rows(self):
        self.assertEqual(rows_from_pairs(SITE, [(0, 1, 2, [4])]), [])

    def test_the_engines_limits_are_refused_not_truncated(self):
        five = [(1, 2, 1)] + [(f, 2, 1) for f in (4, 5, 6, 7)]
        self.assertTrue(rows_fit(SITE, five))
        extra = SITE + [("ch4", "miro", 11)]
        self.assertFalse(rows_fit(extra, five + [(8, 2, 1)]))
        self.assertFalse(rows_fit(SITE, [(4, 2, 1), (4, 0, 1)]))


class TheCppIsWhatThePortSays(unittest.TestCase):
    def setUp(self):
        self.record = read("src/measurement_record.cpp")
        self.header = read("src/measurement_record.h")
        self.model = read("src/cecpairmodel.cpp")

    def test_the_grouping_lines(self):
        for line in (
            "const int water = MeasurementRecords::resolvedCecWater(",
            "if (group.octantReported && group.waterReported) { group.pair.meth = 1; }",
            "else if (group.waterReported) { group.pair.meth = 2; }",
            "else if (group.octantReported) { group.pair.meth = 3; }",
            "else { group.pair.meth = MeasurementRecords::cecMethExtrasOnly; }",
        ):
            self.assertIn(line, self.record)

    def test_the_expansion_lines(self):
        for line in (
            "if (pair.meth == 0) { continue; }",
            "if (pair.meth == 1 || pair.meth == 3)",
            "if (pair.meth == 1 || pair.meth == 2)",
        ):
            self.assertIn(line, self.record)

    def test_the_limits_are_the_engines(self):
        self.assertIn("inline constexpr int cecMaxPairs = %d;" % MAX_PAIRS, self.header)
        self.assertIn("inline constexpr int cecMaxExtras = %d;" % MAX_EXTRAS, self.header)
        self.assertIn("inline constexpr int cecMethExtrasOnly = 4;", self.header)
        typedef = ENGINE / "src/src_common/m_typedef.f90"
        if not typedef.exists():
            self.skipTest("eddyflow-engine is not checked out beside this repo")
        text = typedef.read_text(encoding="utf-8", errors="replace")
        self.assertIn("MaxNumCecPairs = %d" % MAX_PAIRS, text)
        self.assertIn("MaxNumCecExtra = %d" % MAX_EXTRAS, text)

    def test_every_edit_is_checked_and_then_rebuilt_from_the_pairings(self):
        self.assertIn("if (!MeasurementRecords::cecRowsFit(gases, candidate, &reason))",
                      self.model)
        self.assertIn("gases, MeasurementRecords::cecPairsFromRows(gases, candidate));",
                      self.model)
        self.assertIn("emit editRefused(reason);", self.model)
        self.assertIn("&CecPairModel::editRefused", read("src/cecsettingsdialog.cpp"))

    def test_a_new_flux_brings_its_own_analysers_water_and_octant_gas(self):
        self.assertIn("row.waterIndex = MeasurementRecords::defaultCecWater(gases, picked);",
                      self.model)
        self.assertIn("row.octantIndex = MeasurementRecords::defaultCecOctant(gases, picked);",
                      self.model)

    def test_co2_octants_are_the_default_wherever_there_is_co2(self):
        block = self.record[self.record.index("int defaultCecOctant("):]
        block = block[:block.index("\n}\n")]
        self.assertLess(block.index("firstRecordOf(gases, co2)"),
                        block.index("recordIs(gases, fluxIndex, cos)"))

    def test_an_unknown_meth_is_read_as_on_like_the_engine_does(self):
        self.assertIn("if (pair.meth < 0 || pair.meth > MeasurementRecords::cecMethExtrasOnly)",
                      read("src/ecproject.cpp"))


class CarbonylSulfideIsLabelledExperimental(unittest.TestCase):
    def setUp(self):
        self.model = read("src/cecpairmodel.cpp")
        self.dialog = read("src/cecsettingsdialog.cpp")

    def test_the_octant_gas_and_the_flux_say_so_in_the_cell(self):
        self.assertIn(r'tr("%1 \xe2\x80\x93 experimental")', self.model)
        self.assertIn('QStringLiteral("cos")', self.model)

    def test_the_rule_is_one_function(self):
        record = read("src/measurement_record.cpp")
        block = record[record.index("bool isCecExperimental("):]
        block = block[:block.index("\n}\n")]
        self.assertIn('recordIs(gases, row.octantIndex, QStringLiteral("cos"))', block)
        self.assertIn('!recordIs(gases, row.fluxIndex, QStringLiteral("co2"))', block)

    def test_the_note_under_the_table_follows_the_rows(self):
        self.assertIn("experimentalLabel->setVisible(pairModel->anyExperimental());",
                      self.dialog)
        self.assertIn("experimentalLabel->hide();", self.dialog)

    def test_the_tooltips_say_how_to_read_the_cos_columns(self):
        for text in ("cos_stomatal_cec", "cos_nonstomatal_cec", "r_cos_cec",
                     "cec_sign_cos", "senescent", "relative uptake",
                     "Zahn et al. (2022)"):
            self.assertIn(text, self.model)
        #> And the COS-octant caveat, where the choice is made.
        self.assertIn("Octants from carbonyl sulfide", self.model)
        self.assertIn("ground is a net COS source", self.model)

    def test_the_group_names_the_published_method_and_the_experiment(self):
        block = self.dialog[self.dialog.index('new QGroupBox(tr("Fluxes to partition")'):]
        block = block[:block.index("pairModel = new CecPairModel")]
        self.assertIn("Zahn et al. 2022", block)
        self.assertIn("<b>experimental</b>", block)


class TheFormatMovedWithBothRepositories(unittest.TestCase):
    def test_gui_and_engine_agree_on_5_2_0(self):
        self.assertIn('PROJECT_FILE_VERSION_STR = QStringLiteral("5.2.0")',
                      read("src/defs.h"))
        globals_ = ENGINE / "src/src_common/m_common_global_var.f90"
        if not globals_.exists():
            self.skipTest("eddyflow-engine is not checked out beside this repo")
        supported = re.search(r"MaxSupportedIniVer = '([\d.]+)'",
                              globals_.read_text(encoding="utf-8", errors="replace"))
        self.assertEqual(supported.group(1), "5.2.0")

    def test_the_engine_accepts_the_same_octant_gases(self):
        resolver = ENGINE / "src/src_common/gas_slot_resolution.f90"
        if not resolver.exists():
            self.skipTest("eddyflow-engine is not checked out beside this repo")
        text = resolver.read_text(encoding="utf-8", errors="replace")
        self.assertIn("slot = CecSlotOfRecord(rec, 'CO2')", text)
        self.assertIn("if (slot == 0) slot = CecSlotOfRecord(rec, 'COS')", text)
        record = read("src/measurement_record.cpp")
        self.assertIn('return recordIs(gases, recordIndex, QStringLiteral("co2"))', record)
        self.assertIn('|| recordIs(gases, recordIndex, QStringLiteral("cos"));', record)


if __name__ == "__main__":
    unittest.main()
