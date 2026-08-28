"""Biomet labels are matched whole, not by substring.

The FLUXNET names never matched: "SW_IN_1_1_1".contains("RG") is false, and
"LW_IN_1_1_1".contains("LWIN") is false because of the underscore. So a dataset
carrying SW_IN and LW_IN - which the LI-COR demo GHG data does - got no Global
Radiation and no Longwave Incoming row, and neither could be chosen for flux
correction.

Two places dropped them, not one. The reader filtered on read, before the page
ever saw the variable, and it filtered backwards: a list of allowed ids was
searched for entries CONTAINING the extracted type, so bare "P" and "G" were
admitted through "PA" and "RG" while SW_IN, which nothing contains, was refused.

The alias sets are the engine's, from biomet_enrich_vars_description.f90, so the
two agree on what a name means. RG and SW_IN are one measurement there and here:
the engine writes both out as SW_IN.
"""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT.parent / "eddyflow-engine" / "src" / "src_rp" / "biomet_enrich_vars_description.f90"


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def body(source, signature):
    """One function's body, so a match cannot come from elsewhere in the file."""
    start = source.index(signature)
    return source[start:source.index("\n}\n", start)]


def code(fn):
    """A function body with its comment lines dropped, so an assertion that
    something is absent is not defeated by a comment explaining why."""
    return "\n".join(l for l in fn.splitlines()
                     if not l.lstrip().startswith("//"))


def var_type_body():
    return body(read("src/biommetadatareader.cpp"),
                "BiomMetadataReader::VarType BiomMetadataReader::varType")


def alias_set(name):
    """One QSet initialiser out of varType, so an assertion about the PAR set
    cannot be satisfied by a name in the radiation set."""
    fn = var_type_body()
    start = fn.index(name + " {")
    return fn[start:fn.index("};", start)]


class TheAliasSetsMatchTheEngine(unittest.TestCase):
    """A spelling the page offers that the engine does not know is a column
    selected for a correction that will then not happen."""

    def test_global_radiation_carries_every_engine_spelling(self):
        radiation = alias_set("globalRadiation")
        for alias in ("RG", "R_G", "RGLOBAL", "R_GLOBAL", "SWIN", "SW_IN"):
            self.assertIn('"%s"' % alias, radiation, alias)

    def test_longwave_carries_both_spellings(self):
        longwave = alias_set("longwaveIncoming")
        for alias in ("LWIN", "LW_IN"):
            self.assertIn('"%s"' % alias, longwave, alias)

    def test_the_engine_still_groups_rg_with_sw_in(self):
        """The synonym set is only right while the engine agrees."""
        if not ENGINE.exists():
            self.skipTest("engine repo not checked out beside this one")
        engine = ENGINE.read_text(encoding="utf-8", errors="replace")
        self.assertIn("'RG', 'R_G', 'RGLOBAL','R_GLOBAL', 'SWIN','SW_IN'", engine)
        self.assertIn("'LWIN','LW_IN'", engine)


class MatchingIsWholeNameNotSubstring(unittest.TestCase):
    def test_the_page_no_longer_tests_by_contains(self):
        fn = body(read("src/basicsettingspage.cpp"),
                  "void BasicSettingsPage::parseBiomMetadata")
        self.assertNotIn("type_.contains(", fn)
        self.assertIn("BiomMetadataReader::varType(bi.type_)", fn)

    def test_the_reader_asks_the_question_the_right_way_round(self):
        """The filter it replaces kept an entry when an allowed id contained
        the extracted type, which let single letters through."""
        fn = body(read("src/biommetadatareader.cpp"),
                  "bool BiomMetadataReader::readEmbMetadata")
        self.assertNotIn(".filter(", code(fn))
        self.assertIn("varType(var) == VarType::Unknown", fn)

    def test_the_base_name_strips_only_numeric_tail_groups(self):
        """Splitting on "_" cannot do it: the name itself may contain
        underscores, and SW_IN with no positional qualifier must survive."""
        fn = body(read("src/biommetadatareader.cpp"),
                  "QString BiomMetadataReader::baseName")
        self.assertIn("lastIndexOf(QLatin1Char('_'))", fn)
        self.assertIn("toInt(&isNumber)", fn)
        self.assertIn("truncate(underscore)", fn)

    def test_the_external_path_keeps_the_whole_header_field(self):
        """It used to take only the first underscore-separated piece, which
        turned SW_IN_1_1_1 into SW and collapsed TA_1_1_1 with TA_1_3_1."""
        fn = body(read("src/biommetadatareader.cpp"),
                  "bool BiomMetadataReader::readAltMetadata")
        self.assertIn("strings.at(k).trimmed()", fn)
        self.assertNotIn("var.at(0).toUpper()", code(fn))
        self.assertNotIn(".filter(", code(fn))


class TheOutgoingCounterpartsAreNotOffered(unittest.TestCase):
    """PPFD_OUT contains PPFD but is a different quantity - the engine files it
    as PPFD_R. Substring matching offered it as incoming PAR."""

    def test_only_the_incoming_par_names_are_listed(self):
        par = alias_set("par")
        self.assertIn('"PPFD"', par)
        self.assertIn('"PPFD_IN"', par)
        for absent in ("PPFD_OUT", "PPFD_R", "PPFD_DIF"):
            self.assertNotIn(absent, par, absent)

    def test_no_outgoing_radiation_name_is_in_any_set(self):
        fn = var_type_body()
        for absent in ("SW_OUT", "LW_OUT", "SWOUT", "LWOUT", "SW_DIF", "SW_DIR"):
            self.assertNotIn('"%s"' % absent, fn, absent)


if __name__ == "__main__":
    unittest.main()
