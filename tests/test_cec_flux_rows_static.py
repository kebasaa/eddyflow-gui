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
    #> Canonical order: by octant record, then by the water read - never by
    #> the order the rows arrive in.
    groups.sort(key=lambda g: (g["octant"], g["water"]))
    pairs = []
    for g in groups:
        meth = (1 if g["own"] and g["wet"] else 2 if g["wet"]
                else 3 if g["own"] else 4)
        pairs.append((meth, g["octant"], g["cell"], g["extras"]))
    return pairs, repeated


def label(gases, index):
    slug, instrument, _ = gases[index - 1]
    text = slug.upper()
    return text + (" (%s)" % instrument if real(instrument) else "")


def sort_for_display(gases, rows):
    """Alphabetically by flux, then octant gas, then the water read; a
    channel only the engine resolves (index 0) after every named one."""
    def key(index):
        if index < 1:
            return (1, "", 0)
        return (0, label(gases, index).lower(), index)

    return sorted(rows, key=lambda r: (key(r[0]), key(r[2]),
                                       key(resolved_water(gases, r[2], r[1]))))


def normalised(gases, rows):
    return sort_for_display(gases, rows_from_pairs(gases, group_rows(gases, rows)[0]))


def next_free_variant(gases, rows, flux, water, octant):
    own = gases[flux - 1][1] if flux > 0 and real(gases[flux - 1][1]) else None

    def channels(slug, on_own):
        return [i for i, (s, instr, col) in enumerate(gases, start=1)
                if col > 0 and s == slug and ((own is not None and instr == own) == on_own)]

    octants = [octant]
    for slug, on_own in (("co2", True), ("cos", True), ("co2", False), ("cos", False)):
        octants += [i for i in channels(slug, on_own) if i not in octants]
    waters = [water]
    for on_own in (True, False):
        waters += [i for i in channels("h2o", on_own) if i not in waters]
    for w in waters:
        for o in octants:
            if o > 0 and rows_fit(gases, rows + [(flux, w, o)]):
                return (flux, w, o)
    return None


def pair_suffixes(gases, pairs):
    """The engine's CecPairs suffix rule, as cecPairSuffixes mirrors it."""
    def instrument(index):
        if index < 1 or index > len(gases) or not real(gases[index - 1][1]):
            return ""
        return gases[index - 1][1].lower()

    bases = []
    for n, (_, octant, _, _) in enumerate(pairs, start=1):
        base = instrument(octant) or "pair%d" % n
        if 1 <= octant <= len(gases) and gases[octant - 1][0] != "co2":
            base += "_" + gases[octant - 1][0]
        bases.append(base)
    names = []
    for base, (_, octant, water, _) in zip(bases, pairs):
        w = instrument(resolved_water(gases, octant, water))
        names.append(base + "_h2o_" + w
                     if bases.count(base) > 1 and w and w != instrument(octant)
                     else base)
    return ["_" + n if names.count(n) == 1 else "_%s_%d" % (n, names[:i + 1].count(n))
            for i, n in enumerate(names)]


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


#: The default table for SITE, as the model shows it.
DEFAULT_ROWS = normalised(SITE, [(1, 2, 1), (2, 2, 1), (6, 7, 6), (7, 7, 6)])


class OneGasSplitSeveralWays(unittest.TestCase):
    """COS by CO2/H2O octants and by COS/H2O octants, side by side - the
    point of the whole table - and with another analyser's water too."""

    def test_two_versions_of_one_flux_coexist(self):
        rows = DEFAULT_ROWS + [(4, 2, 1), (4, 2, 4)]
        self.assertTrue(rows_fit(SITE, rows))
        pairs, _ = group_rows(SITE, rows)
        #> In canonical order: by octant record, so COS (4) before the
        #> LI-7200's CO2 (6).
        self.assertEqual(pairs, [(1, 1, 2, [4]), (3, 4, 2, []), (1, 6, 7, [])])

    def test_duplicate_moves_to_the_own_analysers_cos_octants(self):
        #> Not to the LI-7200's CO2: that would only add a time-lag mismatch.
        rows = DEFAULT_ROWS + [(4, 2, 1)]
        self.assertEqual(next_free_variant(SITE, rows, 4, 2, 1), (4, 2, 4))
        rows.append((4, 2, 4))
        #> Then the LI-7200's CO2 octants with the MIRO's water, and only
        #> then another water.
        self.assertEqual(next_free_variant(SITE, rows, 4, 2, 1), (4, 2, 6))

    def test_add_goes_on_when_every_flux_has_a_row(self):
        rows = DEFAULT_ROWS + [(4, 2, 1), (5, 2, 1)]
        taken = {r[0] for r in rows}
        self.assertTrue(all(i in taken for i, g in enumerate(SITE, 1) if g[2] > 0))
        self.assertIsNotNone(next_free_variant(SITE, rows, 4, 2, 1))

    def test_versions_sit_together(self):
        rows = normalised(SITE, DEFAULT_ROWS + [(4, 2, 4), (4, 2, 1), (4, 7, 1)])
        fluxes = [r[0] for r in rows]
        first = fluxes.index(4)
        self.assertEqual(fluxes[first:first + 3], [4, 4, 4])

    def test_the_table_is_alphabetical(self):
        #> By flux label, then octant gas label, then water label: CO2 before
        #> COS before H2O, the LI-7200 before the MIRO, CO2 octants before COS.
        rows = normalised(SITE, DEFAULT_ROWS + [(4, 2, 4), (4, 7, 1), (4, 2, 1)])
        self.assertEqual(rows, [
            (6, 7, 6),   # CO2 (li7200)
            (1, 2, 1),   # CO2 (miro)
            (4, 7, 1),   # COS (miro), CO2 (miro) octants, H2O (li7200)
            (4, 2, 1),   # COS (miro), CO2 (miro) octants, H2O (miro)
            (4, 2, 4),   # COS (miro), COS (miro) octants
            (7, 7, 6),   # H2O (li7200)
            (2, 2, 1),   # H2O (miro)
        ])

    def test_the_round_trip_does_not_depend_on_row_order(self):
        rows = DEFAULT_ROWS + [(4, 2, 4), (4, 7, 1), (5, 2, 4)]
        self.assertEqual(normalised(SITE, rows), normalised(SITE, list(reversed(rows))))
        once = normalised(SITE, rows)
        self.assertEqual(normalised(SITE, once), once)

    def test_each_version_gets_its_own_suffix(self):
        #> base_cec_cos_octants, whose expected names the engine's own check
        #> holds too.
        pairs = [(1, 1, 2, [4, 5, 6, 7]), (1, 6, 7, []), (1, 4, 2, [5]), (4, 4, 7, [5])]
        self.assertEqual(pair_suffixes(SITE, pairs),
                         ["_miro", "_li7200", "_miro_cos", "_miro_cos_h2o_li7200"])
        #> Two versions sorted by the MIRO's CO2, one with each water.
        self.assertEqual(pair_suffixes(SITE, [(1, 1, 2, [4]), (4, 1, 7, [4])]),
                         ["_miro", "_miro_h2o_li7200"])


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
        self.assertIn("gases, MeasurementRecords::cecPairsFromRows(gases, rows)));",
                      self.model)
        self.assertIn("const auto rows = normalised(candidate);", self.model)
        self.assertIn("emit editRefused(reason);", self.model)
        self.assertIn("&CecPairModel::editRefused", read("src/cecsettingsdialog.cpp"))

    def test_a_new_flux_brings_its_own_analysers_water_and_octant_gas(self):
        #> Unless that version is already in the table: then the row keeps its
        #> own water and octant gas, and failing that takes the next free
        #> combination - choosing a gas twice is how its second version is made.
        block = self.model[self.model.index("        case Flux:\n        {"):]
        block = block[:block.index("            row = next;")]
        self.assertIn("MeasurementRecords::defaultCecWater(gases, picked),", block)
        self.assertIn("MeasurementRecords::defaultCecOctant(gases, picked)};", block)
        self.assertLess(block.index("if (!fits(byDefault))"),
                        block.index("if (fits(kept)) { next = kept; }"))
        self.assertLess(block.index("if (fits(kept)) { next = kept; }"),
                        block.index("MeasurementRecords::cecNextFreeVariant("))

    def test_add_and_duplicate_search_for_a_free_version(self):
        add = self.model[self.model.index("int CecPairModel::addPair()"):]
        add = add[:add.index("\n}\n")]
        self.assertIn("MeasurementRecords::cecNextFreeVariant(", add)
        self.assertNotIn("if (taken.contains(flux)) { continue; }", add)
        dup = self.model[self.model.index("int CecPairModel::duplicatePair(int row)"):]
        dup = dup[:dup.index("\n}\n")]
        self.assertIn("from.waterIndex, from.octantIndex,", dup)
        dialog = read("src/cecsettingsdialog.cpp")
        self.assertIn('duplicatePairButton = new QPushButton(tr("Duplicate"), pairGroup);', dialog)
        self.assertIn("duplicatePairButton->setEnabled(selected);", dialog)

    def test_the_search_prefers_the_flux_own_analyser(self):
        block = self.record[self.record.index("bool cecNextFreeVariant("):]
        block = block[:block.index("\n}\n")]
        order = ["add(octants, co2, true);", "add(octants, cos, true);",
                 "add(octants, co2, false);", "add(octants, cos, false);",
                 "add(waters, kH2o, true);", "add(waters, kH2o, false);"]
        at = [block.index(line) for line in order]
        self.assertEqual(at, sorted(at))

    def test_the_canonical_order_and_the_display_sort(self):
        self.assertIn("return left.pair.carbonIndex < right.pair.carbonIndex;", self.record)
        self.assertIn("return left.water < right.water;", self.record)
        block = self.record[self.record.index("QVector<CecFluxRow> sortCecRowsForDisplay("):]
        block = block[:block.index("\n}\n")]
        flux = block.index("if (const int c = compare(l.fluxIndex, r.fluxIndex)) { return c < 0; }")
        octant = block.index("if (const int c = compare(l.octantIndex, r.octantIndex)) { return c < 0; }")
        water = block.index("const int c = compare(resolvedCecWater(gases, l.octantIndex, l.waterIndex),")
        self.assertLess(flux, octant)
        self.assertLess(octant, water)
        self.assertIn("Qt::CaseInsensitive", block)

    def test_an_edited_row_moves_with_its_selection(self):
        apply = self.model[self.model.index("bool CecPairModel::apply("):]
        apply = apply[:apply.index("\n}\n")]
        self.assertLess(apply.index("emit layoutAboutToBeChanged();"),
                        apply.index("changePersistentIndexList(before, after);"))
        self.assertLess(apply.index("changePersistentIndexList(before, after);"),
                        apply.index("emit layoutChanged();"))

    def test_the_suffix_column_mirrors_the_engine(self):
        block = self.record[self.record.index("QStringList cecPairSuffixes("):]
        block = block[:block.index("\n}\n")]
        self.assertIn('base = QStringLiteral("pair%1").arg(i + 1);', block)
        self.assertIn('name += QStringLiteral("_h2o_") + water;', block)
        self.assertIn('out.append(QStringLiteral("_%1_%2").arg(names.at(i)).arg(occurrence));',
                      block)
        self.assertIn('case Suffix: return tr("Columns end in");', self.model)
        resolver = ENGINE / "src/src_common/gas_slot_resolution.f90"
        if not resolver.exists():
            self.skipTest("eddyflow-engine is not checked out beside this repo")
        text = resolver.read_text(encoding="utf-8", errors="replace")
        self.assertIn("names(i) = trim(bases(i)) // '_h2o_' // trim(base)", text)

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
                     "cec_noisy_octants", "Warning 124",
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
