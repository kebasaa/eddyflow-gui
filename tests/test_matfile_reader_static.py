"""The MAT-file reader, and the four things a binary parser has to get right.

``src/matfile.cpp`` reads MATLAB level 5 files - the "v7" flavour, in which
every top-level variable is a separately zlib-compressed element. It exists so
the interface can import an EddyUH project, which is four MATLAB files.

Parsing a binary format written by another program, from a path the user
chose, is the most hostile input this application takes. So:

1. **A compressed element is not padded to eight bytes.** The specification
   says data elements are, and MATLAB's own writer does not for this one.
   Assuming otherwise desynchronises the walk at the *second* variable, so a
   one-variable file reads perfectly and a project file reads as rubbish.
   Verified below against the real files, byte by byte, without going near the
   C++ - if a future MATLAB starts padding, this says so.
2. **The small data element form exists.** Up to four bytes packed into the
   tag, flagged by a non-zero upper half-word. Missing it reads a field name
   as a type code.
3. **v7.3 is refused by name.** It is HDF5 wearing a level 5 header; read as
   level 5 it produces confident nonsense rather than an error.
4. **Every read is bounds-checked**, and the first failure abandons the file
   rather than returning what was read so far.

Also pinned: **integer classes are promoted to double.** MATLAB stores a whole
number in the narrowest class that fits, so the same field is ``uint8`` in one
project and ``double`` in another purely because of the value it holds. An
EddyUH analyser's ``lags`` is ``uint8`` on one instrument and ``float64`` on
the next, in one file, for that reason alone - and both are in seconds. Taking
the width to mean something is how that becomes a lag in samples.
"""

import re
import struct
import unittest
import zlib
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
HEADER = GUI_ROOT / "src" / "matfile.h"
SOURCE = GUI_ROOT / "src" / "matfile.cpp"
SOURCES_PRI = GUI_ROOT / "sources.pri"
LIBS_PRI = GUI_ROOT / "libs.pri"

#> The supplied EddyUH project, which lives outside the repository: it carries
#> a site's coordinates and one machine's absolute paths, and committing it to
#> get a fixture would be a poor trade. Every check that needs it skips.
EDDYUH = GUI_ROOT.parent / "EddyUH_testing" / "EddyUH"
FIXTURES = [
    EDDYUH / "preproc_2603231128_CH-",
    EDDYUH / "lag_2603231128_CH-.10cl",
    EDDYUH / "planar_fit_2603231128_CH-.1cl",
]
have_fixtures = all(p.is_file() for p in FIXTURES)


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


SRC = read(SOURCE)
HDR = read(HEADER)

MI_COMPRESSED = 15
MI_MATRIX = 14


def walk(buf, padded):
    """Top-level elements, advancing with or without padding."""
    out = []
    off = 128
    while off + 8 <= len(buf):
        dtype, nbytes = struct.unpack_from("<II", buf, off)
        out.append((off, dtype, nbytes))
        step = 8 + nbytes
        if padded:
            step += (8 - nbytes % 8) % 8
        off += step
        if len(out) > 4096:
            break
    return out, off


class TheCompressedElementsAreNotPadded(unittest.TestCase):
    """Property 1, and the one that is easy to get wrong and hard to notice."""

    @unittest.skipUnless(have_fixtures, "the EddyUH project is not present")
    def test_the_unpadded_walk_lands_exactly_on_the_end_of_each_file(self):
        for p in FIXTURES:
            buf = p.read_bytes()
            _, end = walk(buf, padded=False)
            self.assertEqual(end, len(buf),
                             "%s: the unpadded walk must consume the file "
                             "exactly" % p.name)

    @unittest.skipUnless(have_fixtures, "the EddyUH project is not present")
    def test_the_padded_walk_does_not(self):
        #> Otherwise the property above is vacuous - it would hold for a file
        #> whose every element happened to be a multiple of eight.
        overshot = 0
        for p in FIXTURES:
            buf = p.read_bytes()
            _, end = walk(buf, padded=True)
            if end != len(buf):
                overshot += 1
        self.assertEqual(overshot, len(FIXTURES),
                         "padding every element must break every file, or "
                         "these files cannot tell the two rules apart")

    @unittest.skipUnless(have_fixtures, "the EddyUH project is not present")
    def test_at_least_one_file_has_more_than_one_variable(self):
        #> A single-variable file reads correctly under either rule, so the
        #> distinction is only testable against a file like preproc.
        counts = []
        for p in FIXTURES:
            elems, _ = walk(p.read_bytes(), padded=False)
            counts.append(len(elems))
        self.assertGreater(max(counts), 1)

    @unittest.skipUnless(have_fixtures, "the EddyUH project is not present")
    def test_every_top_level_element_is_a_compressed_array(self):
        for p in FIXTURES:
            elems, _ = walk(p.read_bytes(), padded=False)
            for off, dtype, _ in elems:
                self.assertEqual(dtype, MI_COMPRESSED,
                                 "%s at %d" % (p.name, off))

    @unittest.skipUnless(have_fixtures, "the EddyUH project is not present")
    def test_each_one_inflates_to_an_array(self):
        for p in FIXTURES:
            buf = p.read_bytes()
            for off, _, nbytes in walk(buf, padded=False)[0]:
                raw = zlib.decompress(buf[off + 8:off + 8 + nbytes])
                dtype, _ = struct.unpack_from("<II", raw, 0)
                self.assertEqual(dtype, MI_MATRIX, p.name)

    def test_the_reader_says_so_in_words(self):
        #> The next person to read the specification will want to know this
        #> was a decision and not an oversight.
        self.assertIn("NOT padded to eight bytes", SRC)
        self.assertIn("miCOMPRESSED", SRC)

    def test_the_reader_pads_everything_else(self):
        block = SRC[SRC.index("const qint64 next ="):]
        block = block[:block.index("}")]
        self.assertIn("tag.type == miCOMPRESSED", block)
        self.assertIn("(8 - tag.bytes % 8) % 8", block)


class TheSmallDataElementFormIsHandled(unittest.TestCase):
    """Property 2."""

    def test_the_tag_reader_checks_the_upper_half_word(self):
        block = SRC[SRC.index("bool MatFileReader::readTag"):]
        block = block[:block.index("\n}")]
        self.assertIn("(first >> 16) != 0", block)
        self.assertIn("first & 0xffff", block)
        self.assertIn("headerBytes = 4", block)

    def test_a_compact_tag_cannot_claim_more_than_four_bytes(self):
        self.assertIn("tag->bytes > 4", SRC)

    @unittest.skipUnless(have_fixtures, "the EddyUH project is not present")
    def test_the_real_files_actually_use_it(self):
        #> If they did not, the code above would be untested by any run
        #> against them and this suite would be claiming coverage it lacks.
        found = False
        for p in FIXTURES:
            buf = p.read_bytes()
            for off, _, nbytes in walk(buf, padded=False)[0]:
                raw = zlib.decompress(buf[off + 8:off + 8 + nbytes])
                for at in range(0, min(len(raw), 4096), 4):
                    word, = struct.unpack_from("<I", raw, at)
                    if (word >> 16) != 0 and 1 <= (word & 0xffff) <= 18:
                        found = True
                        break
        self.assertTrue(found,
                        "no compact tag anywhere in the fixtures")


class TheDangerousInputsAreRefused(unittest.TestCase):
    """Properties 3 and 4."""

    def test_v73_is_named_rather_than_misread(self):
        self.assertIn(r'\x89HDF', SRC)
        self.assertIn("v7.3", SRC)
        #> And says what to do about it.
        self.assertIn("-v7", SRC)

    def test_a_big_endian_file_is_refused_rather_than_byte_swapped(self):
        #> Untested code that swaps every read would be worse than an honest
        #> refusal of a case no machine here produces.
        self.assertIn('QByteArrayLiteral("IM")', SRC)
        self.assertIn("big-endian", SRC)

    def test_the_header_is_checked_before_anything_is_parsed(self):
        at_header = SRC.index("MATLAB 5.0 MAT-file")
        at_parse = SRC.index("reader.parseTop(")
        self.assertLess(at_header, at_parse)

    def test_a_failed_parse_discards_what_it_read(self):
        #> order_ and vars_ are only assigned after parseTop returns true, so
        #> a half-read file cannot be offered as a whole one.
        block = SRC[SRC.index("if (!reader.parseTop("):]
        self.assertLess(block.index("return false;"), block.index("order_ ="))

    def test_every_declared_length_is_checked_against_the_buffer(self):
        for guard in ("at + 8 > b.size()",
                      "at + 8 + static_cast<qint64>(tag->bytes) > b.size()",
                      "start + static_cast<qint64>(tag.bytes) > limit",
                      "end > b.size()"):
            self.assertIn(guard, SRC, guard)

    def test_inflation_is_bounded(self):
        #> A compressed element declares only its compressed size, so nothing
        #> in the file limits what it expands to.
        self.assertIn("kMaxInflated", SRC)
        self.assertIn("payload.size() > kMaxInflated", SRC)

    def test_a_struct_cannot_declare_an_absurd_field_name_length(self):
        self.assertIn("width <= 0 || width > 256", SRC)

    def test_a_negative_dimension_is_refused(self):
        self.assertIn("rows < 0 || columns < 0", SRC)

    def test_objects_and_sparse_arrays_are_refused_by_name(self):
        for k in ("mxOBJECT", "mxSPARSE"):
            self.assertIn(k, SRC)
        self.assertIn("does not understand", SRC)

    def test_floats_are_not_type_punned(self):
        #> A reinterpret_cast between a float and its bytes is undefined; the
        #> memcpy compiles to the same move and is not.
        self.assertIn("std::memcpy(&f, &bits, sizeof f)", SRC)
        self.assertIn("std::memcpy(&v, &bits, sizeof v)", SRC)


class ItIsReadOnly(unittest.TestCase):

    def test_there_is_no_writer(self):
        #> The importer reads an EddyUH project and writes an EddyFlow one.
        #> Nothing should ever write back over a user's MATLAB file.
        for forbidden in ("QIODevice::WriteOnly", "QIODevice::Append",
                          "deflate(", "deflateInit"):
            self.assertNotIn(forbidden, SRC, forbidden)

    def test_the_only_file_mode_is_read_only(self):
        self.assertEqual(SRC.count("f.open("), 1)
        self.assertIn("f.open(QIODevice::ReadOnly)", SRC)


class TheWidthOfAnIntegerMeansNothing(unittest.TestCase):
    """The trap this reader is shaped to avoid."""

    def test_every_class_is_promoted_to_double(self):
        self.assertIn("QVector<double>* out", SRC)
        #> No accessor hands back the original width, so no caller can branch
        #> on it even by accident.
        self.assertNotIn("MiType type() const", HDR)
        self.assertNotIn("originalType", HDR)

    def test_the_header_says_why(self):
        self.assertIn("narrowest type that fits", HDR)
        self.assertIn("samples", HDR)

    @unittest.skipUnless(have_fixtures, "the EddyUH project is not present")
    def test_the_fixture_really_does_store_one_field_two_ways(self):
        #> set_Gan(1).lags is uint8 and set_Gan(2).lags is double, in the same
        #> file, for the same field. If this ever stops being true the note
        #> above stops being motivated by evidence.
        import numpy as np
        from scipy.io import loadmat
        m = loadmat(str(FIXTURES[0]), squeeze_me=False, struct_as_record=True)
        gan = m["set_Gan"]
        kinds = {gan[0, i]["lags"].dtype.kind for i in range(gan.shape[1])}
        self.assertEqual(kinds, {"u", "f"},
                         "expected one integer and one floating lags array")


class ItIsInTheBuild(unittest.TestCase):

    def test_both_files_are_listed(self):
        pri = read(SOURCES_PRI)
        self.assertIn("src/matfile.h", pri)
        self.assertIn("src/matfile.cpp", pri)

    def test_zlib_is_named_rather_than_arriving_by_accident(self):
        #> QuaZip drags it in, so this links either way - until someone drops
        #> QuaZip, or the linker reorders, and then it does not.
        libs = read(LIBS_PRI)
        self.assertIn("LIBS += -lz", libs)
        #> Outside the debug/release branches, so both get it.
        head = libs[:libs.index("CONFIG(debug, debug|release)")]
        self.assertIn("-lz", head)


if __name__ == "__main__":
    unittest.main()
