"""Which analysers let you type in their own path lengths, and why.

The engine reads `hpath_length`, `vpath_length` and `tau` out of the .metadata
file for one specific set of models - the `select case` in
`read_metadata_file.f90`. Every other model has its geometry hardcoded in
`retrieve_sensor_params.f90`, so the interface greys those three cells out: a
number you can type but that is then ignored is worse than no field at all.

That set has grown. The interface's copy of it did not grow with it, and it was
not one copy but three, spread over twelve `if` statements:

* the DisplayRole cases listed all fifteen models - correct;
* the EditRole, BackgroundRole and every delegate case dropped `csi_ec150`
  and `csi_irgason_irga`;
* `IrgaModel::flags()` alone dropped `aerodyne_tildas` as well.

So a TILDAS got a spin box built for it and a value printed into the cell, while
`flags()` cleared `ItemIsEnabled` and made that cell unreachable; an EC150 or
IRGASON showed a value nothing could change. Meanwhile the engine read whatever
was in the file for all three.

The lists are therefore gone, replaced by `IrgaDesc::needsPathGeometry` and, for
the two extinction-coefficient columns that had their own ten-fold copy,
`IrgaDesc::hasExtinctionCoefficients`. This pins both sets and, more
importantly, pins the *absence* of a hand-written list to drift out of step
again.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def body(source, signature):
    """The one function, not whatever follows it."""
    start = source.index(signature)
    return source[start:source.index("\n}\n", start)]


IRGA = read("src/irga_desc.cpp")
IRGA_H = read("src/irga_desc.h")
MODEL = read("src/irga_model.cpp")
DELEGATE = read("src/irga_delegate.cpp")
PROJECT = read("src/dlproject.cpp")

#: The `select case` in read_metadata_file.f90, as engine model keys.
ENGINE_READS_FROM_FILE = {
    "generic_open_path", "generic_closed_path",
    "csi_ec150", "csi_ec155", "csi_irgason_irga", "csi_tga200a",
    "miro_mga1_5", "miro_mga4_6", "miro_mga9_10", "miro_mgai_n2o",
    "aerodyne_tildas",
    "open_path_krypton", "open_path_lyman",
    "closed_path_krypton", "closed_path_lyman",
}

#: The branch of that select case which also reads kw and ko.
ENGINE_READS_EXTINCTION = {
    "open_path_krypton", "open_path_lyman",
    "closed_path_krypton", "closed_path_lyman",
}


def engine_keys(indices):
    """IRGA_MODEL_STRING_<n> -> the key the engine actually matches on."""
    table = dict(re.findall(
        r'IRGA_MODEL_STRING_(\d+) = QStringLiteral\("([a-z0-9_]+)"\)', PROJECT))
    return {table[n] for n in indices}


def named_models(chunk):
    return set(re.findall(r"getIRGA_MODEL_STRING_(\d+)\(\)", chunk))


class ThePredicatesExist(unittest.TestCase):

    def test_both_are_declared(self):
        self.assertIn("static bool needsPathGeometry(const QString& model);",
                      IRGA_H)
        self.assertIn(
            "static bool hasExtinctionCoefficients(const QString& model);",
            IRGA_H)

    def test_they_say_where_the_set_comes_from(self):
        """A list with no cited source is what drifted the first time."""
        note = IRGA[:IRGA.index("bool IrgaDesc::needsPathGeometry")][-800:]
        self.assertIn("read_metadata_file.f90", IRGA_H)
        self.assertIn("read_metadata_file.f90", note)
        self.assertIn("retrieve_sensor_params.f90", note)


class TheSetMatchesTheEngine(unittest.TestCase):
    """Resolved through the DlProject table, so this compares the keys the
    engine matches on rather than two lists of indices that could both be
    wrong."""

    PATH_PREDICATE = "bool IrgaDesc::needsPathGeometry(const QString& model)"
    K_PREDICATE = "bool IrgaDesc::hasExtinctionCoefficients(const QString& model)"

    def test_path_geometry_is_exactly_what_the_engine_reads(self):
        chunk = body(IRGA, self.PATH_PREDICATE)
        self.assertEqual(ENGINE_READS_FROM_FILE, engine_keys(named_models(chunk)))

    def test_the_three_that_were_missing_are_in_it(self):
        """The regression itself: EC150, IRGASON, TILDAS."""
        chunk = body(IRGA, self.PATH_PREDICATE)
        self.assertLessEqual({"csi_ec150", "csi_irgason_irga", "aerodyne_tildas"},
                             engine_keys(named_models(chunk)))

    def test_the_licor_family_stays_out(self):
        """Their geometry is hardcoded engine-side; an editable cell would lie."""
        keys = engine_keys(named_models(body(IRGA, self.PATH_PREDICATE)))
        self.assertEqual(set(), {k for k in keys if k.startswith("li")})

    def test_extinction_is_exactly_the_hygrometers(self):
        chunk = body(IRGA, self.K_PREDICATE)
        self.assertEqual(ENGINE_READS_EXTINCTION, engine_keys(named_models(chunk)))

    def test_no_model_has_appeared_without_being_considered(self):
        """A new analyser added to DlProject is either in this set or
        deliberately not; failing here is how that decision gets made."""
        known = set(re.findall(
            r'IRGA_MODEL_STRING_\d+ = QStringLiteral\("([a-z0-9_]+)"\)', PROJECT))
        self.assertLessEqual(ENGINE_READS_FROM_FILE, known)
        self.assertEqual(24, len(known),
                         "a model was added or removed; decide whether it needs "
                         "path geometry")


class TheTableAsksThroughThePredicates(unittest.TestCase):
    """Adding the predicate but leaving one hand-written copy behind would
    reproduce the original fault exactly."""

    #: Every switch case that must agree about these cells.
    PATH_SITES = 12
    EXTINCTION_SITES = 9

    def test_no_hand_written_list_survives(self):
        for source, name in ((MODEL, "irga_model.cpp"),
                             (DELEGATE, "irga_delegate.cpp")):
            self.assertIsNone(
                re.search(r"!= IrgaDesc::getIRGA_MODEL_STRING_\d+\(\)\s*\n\s*&&",
                          source),
                "%s still tests a model list by hand" % name)

    def test_every_site_routes_through_needs_path_geometry(self):
        found = (MODEL.count("IrgaDesc::needsPathGeometry(")
                 + DELEGATE.count("IrgaDesc::needsPathGeometry("))
        self.assertEqual(self.PATH_SITES, found)

    def test_every_site_routes_through_has_extinction_coefficients(self):
        found = (MODEL.count("IrgaDesc::hasExtinctionCoefficients(")
                 + DELEGATE.count("IrgaDesc::hasExtinctionCoefficients("))
        self.assertEqual(self.EXTINCTION_SITES, found)

    def test_flags_agrees_with_the_editor_and_the_paint(self):
        """flags() is the one that decides whether the cell can be reached at
        all, so its disagreeing is what made the fields unreachable rather than
        merely odd-looking."""
        chunk = body(
            MODEL, "Qt::ItemFlags IrgaModel::flags(const QModelIndex& index) const")
        self.assertIn("!IrgaDesc::needsPathGeometry(irgaDesc.model())", chunk)
        self.assertIn("!IrgaDesc::hasExtinctionCoefficients(irgaDesc.model())",
                      chunk)

    def test_the_validator_uses_the_same_predicate(self):
        """isGoodIrga kept a tenth copy of the hygrometer list."""
        chunk = body(IRGA, "bool IrgaDesc::isGoodIrga(const IrgaDesc &irga)")
        self.assertIn("hasExtinctionCoefficients(model)", chunk)


class TheTubeColumnsAreUntouched(unittest.TestCase):
    """They were already routed through isOpenPathModel; that is the precedent
    this change follows, so it is pinned rather than assumed."""

    def test_they_still_ask_is_open_path_model(self):
        for source in (MODEL, DELEGATE):
            self.assertIn("IrgaDesc::isOpenPathModel(", source)


if __name__ == "__main__":
    unittest.main()
