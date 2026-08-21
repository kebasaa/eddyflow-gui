"""Legacy Campbell model keys must still resolve, on both sides.

The Campbell keys were renamed twice: EddyPro's bare `csat3` and `csat3b`
became `campbell_*`, and those became `csi_*`. No other manufacturer moved, so
a legacy project opens correctly everywhere except on a Campbell site - where
the model matched nothing, the anemometer came back empty, and the master sonic
silently fell back to None.

The interface once carried a rewrite table meant to cover this, in
importEddyProProject. Not one of its fifteen model rows could ever fire: it ran
over the .eddypro *project* file, which holds no instrument models at all; it
anchored `model=` to a line start, while the real metadata key is
`instr_<n>_model=`; and it required the value to end right after the model
name, while every model value carries a trailing `_<n>`. The nine n2o->gas4
rows beside them do work, which is why the table looked functional. The guard
below is against re-adding rows of the first kind.

The engine reads the metadata too - including the copy inside a GHG archive,
which it unzips itself and which nothing on this side can reach - so both
repositories need the same table. Nothing at build time connects them.

Skipped, not failed, when the engine checkout is not beside this one.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_TYPEDEF = ENGINE_ROOT / "src" / "src_common" / "m_typedef.f90"
ENGINE_READ_MD = ENGINE_ROOT / "src" / "src_common" / "read_metadata_file.f90"
ENGINE_DYN_MD = ENGINE_ROOT / "src" / "src_rp" / "retrieve_dynamic_metadata.f90"
DL_PROJECT = GUI_ROOT / "src" / "dlproject.cpp"
DL_PROJECT_HDR = GUI_ROOT / "src" / "dlproject.h"
EC_PROJECT = GUI_ROOT / "src" / "ecproject.cpp"

#: Spellings a file may carry, and what each one means now.
BARE_MODEL_KEYS = {
    "csat3": "csi_csat3",
    "csat3a": "csi_csat3a",
    "csat3b": "csi_csat3b",
    "ec150": "csi_ec150",
}


def _read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def _body(src, start, end="\n}"):
    """The text of one function, by a fragment of its opening line."""
    body = src[src.index(start):]
    return body[: body.index(end)]


class TheNormaliserKnowsEveryLegacySpelling(unittest.TestCase):
    """dlproject's normaliser is the choke point for every metadata source.

    fromIniAnemModel, fromIniIrgaModel and fromIniAnemNorthAlign all resolve
    through it, and so - by way of those two - do the variable table's
    col_<n>_instrument references. It therefore covers the standalone
    metadata, the alternative metadata, and the copy extracted from a GHG.
    """

    def setUp(self):
        self.src = _read(DL_PROJECT)
        self.body = _body(self.src, "QString normalizedCampbellModelKey(")

    def test_the_bare_spellings_are_mapped(self):
        for bare, canonical in BARE_MODEL_KEYS.items():
            self.assertIn(
                'QStringLiteral("%s")' % bare, self.body,
                "%s is a spelling EddyPro or an early EddyFlow wrote" % bare)
            self.assertIn(
                'QStringLiteral("%s")' % canonical, self.body,
                "%s has no current key to map onto" % bare)

    def test_the_prefixed_spelling_is_still_mapped(self):
        # The middle era, from the release where the prefix was the
        # manufacturer's name rather than its abbreviation.
        self.assertIn('QStringLiteral("campbell_")', self.body)
        self.assertIn('QStringLiteral("csi_")', self.body)

    def test_every_metadata_converter_goes_through_it(self):
        for converter in ("fromIniAnemModel", "fromIniIrgaModel",
                          "fromIniAnemNorthAlign"):
            body = _body(self.src, "DlProject::%s(" % converter)
            self.assertIn(
                "normalizedCampbellModelKey(", body,
                "%s resolves a model key without normalising it" % converter)


class LegacyMetadataIsRewrittenOnSave(unittest.TestCase):
    """A tolerated file is still a stale file until something writes it back.

    The read resolves either way, so nothing would ever mark the metadata
    modified and the old key would sit on disk indefinitely. The editor only
    ever loads with firstReading set, which is why that term cannot gate this.
    """

    def setUp(self):
        self.src = _read(DL_PROJECT)

    def test_both_model_reads_are_checked(self):
        self.assertEqual(
            2, self.src.count("!= canonicalModelKey("),
            "the anemometer and the analyser must both be checked")

    def test_the_latch_widens_the_modified_decision(self):
        self.assertIn("(legacyInstrumentSlugs && checkVersion)", self.src)

    def test_the_extracted_ghg_copy_is_left_alone(self):
        # It is scratch, wiped on the next open, and can never be written
        # back, so a dirty flag on it is one nothing can clear. checkVersion
        # is false on exactly that path.
        latch = _body(self.src, "if (legacyInstrumentSlugs && checkVersion)",
                      "\n")
        self.assertIn("checkVersion", latch)

    def test_the_modified_out_parameter_is_null_checked(self):
        # It defaults to nullptr, and this branch is now reachable far more
        # often than the version-compat cases that used to be its only callers.
        self.assertIn("!isVersionCompatible && modified != nullptr", self.src)


class TheProjectFilesOwnModelKeyIsNormalised(unittest.TestCase):
    """master_sonic is the one model key that never met a converter.

    It is read straight out of the ini, then looked up in the master
    anemometer list by exact match. A retired spelling matches nothing and the
    selection falls back to None - the project opens and the sonic is gone.
    """

    def setUp(self):
        self.src = _read(EC_PROJECT)

    def test_master_sonic_is_run_through_the_normaliser(self):
        self.assertIn("DlProject::canonicalModelKey(", self.src)

    def test_the_helper_is_exposed_for_it(self):
        self.assertIn("static QString canonicalModelKey(", _read(DL_PROJECT_HDR))

    def test_it_does_not_borrow_the_record_format_latch(self):
        # wasUpgradedOnLoad_ means "predates the record format" and gates
        # migrateLegacyGasSettings and the month-grouping fold. A slug rename
        # must not drag a modern project through either.
        body = _body(self.src, "const auto sonic = ec_project_state_",
                     "\n        }")
        self.assertNotIn("wasUpgradedOnLoad_", body)
        self.assertIn("isVersionCompatible = false", body)


class TheDeadRewriteRowsStayGone(unittest.TestCase):
    """importEddyProProject rewrites project keys, never instrument models."""

    def setUp(self):
        self.body = _body(_read(EC_PROJECT),
                          "bool EcProject::importEddyProProject(")

    def test_no_model_pattern_is_reintroduced(self):
        self.assertNotIn(
            "model=", self.body,
            "instrument models are not in the project file - a pattern for "
            "them here cannot fire, which is how fifteen of them survived")

    def test_the_working_rows_are_kept(self):
        # These really are top-level [Project] keys, and the patterns built
        # from them have no trailing newline anchor, so they match. The pairs
        # moved into a shared list when the SmartFlux exporter started reading
        # the same list backwards; the rewrite itself is unchanged.
        self.assertIn("fourthGasKeyRenames()", self.body)
        renames = _read(EC_PROJECT)
        for key in ("col_n2o", "sr_lim_n2o", "tl_def_n2o"):
            self.assertIn('QStringLiteral("%s")' % key, renames)


@unittest.skipUnless(ENGINE_TYPEDEF.exists(),
                     "engine checkout not beside this one")
class TheEngineKnowsTheSameSpellings(unittest.TestCase):
    """The engine reads metadata this side never sees.

    It unzips each GHG archive itself and validates the embedded metadata
    against hard-coded model lists, so a retired key there is not a display
    problem - it fails validation and every file in the run is skipped.
    """

    def setUp(self):
        self.src = _read(ENGINE_TYPEDEF)
        self.body = _body(
            self.src,
            "character(32) function CanonicalInstrumentModel(",
            "end function CanonicalInstrumentModel")

    def test_the_tables_agree(self):
        for bare, canonical in BARE_MODEL_KEYS.items():
            self.assertIn("'%s'" % bare, self.body,
                          "the interface maps %s and the engine does not" % bare)
            self.assertIn("'%s'" % canonical, self.body)

    def test_the_prefixed_spelling_is_deliberately_not_mapped(self):
        # Asymmetric on purpose, not an oversight. That prefix is a retired
        # identifier in the engine and its own
        # test_legacy_model_prefixes_are_not_active_identifiers bans the
        # string from the tree, so only this side tolerates it. The exposure
        # is a metadata file written in the one release that spelled it that
        # way: it resolves here and is rejected at processing.
        self.assertNotIn("campbell", self.body)

    def test_the_index_suffix_survives(self):
        # Every consumer strips a trailing `_<n>` with two-character
        # arithmetic, so the rewrite has to put it back.
        self.assertIn("model(model_len - 1:model_len)", self.body)

    def test_every_ingestion_point_normalises(self):
        for path, expected in ((ENGINE_READ_MD, 1), (ENGINE_DYN_MD, 2)):
            self.assertEqual(
                expected, _read(path).count("CanonicalInstrumentModel("),
                "%s does not normalise every model it reads" % path.name)

    def test_the_dynamic_metadata_pass_runs_after_the_field_loop(self):
        # The fields are visited in file-column order, so the model is not
        # guaranteed to have been read at any point during the loop.
        src = _read(ENGINE_DYN_MD)
        self.assertLess(
            src.index("select case (fld)"),
            src.index("CanonicalInstrumentModel("))


if __name__ == "__main__":
    unittest.main()
