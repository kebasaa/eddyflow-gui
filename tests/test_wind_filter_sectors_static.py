"""Wind-direction filter sectors are numbered from 1, as the engine reads them.

The project writer used to count from `wdf_sect_0_*`. The engine's tag
table starts at `wdf_sect_1_start` (SNTags 373, m_rp_global_var.f90) and
ReadIniRP walks `wdf_sect_1_*` .. `wdf_sect_16_*`, so the first excluded sector
matched no tag and was dropped from every run, without a word - and from every
EddyPro export, since EddyPro also counts from 1.

What this file holds in place:

  - **The writer counts from 1**, and clears every `wdf_sect_*` key first. A
    re-saved old project would otherwise keep its `wdf_sect_0_*` beside the
    renumbered keys, and the reader would load it as a duplicate sector.

  - **The reader takes any index**, so a project written from 0 still opens
    with all its sectors - and marks itself modified, because a run saves only
    a modified project and the engine would otherwise go on reading the old
    file one sector short.

  - **The engine's numbering is the one pinned**: tags 1..16, nothing at 0,
    and the same sixteen-sector cap as the table the user edits.

The planar-fit sectors (`pf_sect_*`) are held to the same rules. They never
had either bug - the writer counted from 1 and empties the whole group before
writing - but their table had no cap, and the engine reads only 36.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]

PROJECT = "src/ecproject.cpp"
INIDEFS = "src/ecinidefs.h"
TABLE_H = "src/windfilter_tablemodel.h"
PF_TABLE_H = "src/angle_tablemodel.h"
PF_TABLE = "src/angle_tablemodel.cpp"


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def code(path):
    out = []
    for ln in read(path).splitlines():
        s = ln.lstrip()
        if s.startswith("//") or s.startswith("/*") or s.startswith("*"):
            continue
        out.append(ln)
    return "\n".join(out)


def body(path, signature):
    src = code(path)
    start = src.index(signature)
    depth = 0
    seen = False
    for i in range(start, len(src)):
        if src[i] == "{":
            depth += 1
            seen = True
        elif src[i] == "}":
            depth -= 1
            if seen and depth == 0:
                return src[start:i + 1]
    raise AssertionError("unterminated body for %s" % signature)


def ini_group(block, group):
    """The text between a group's beginGroup and its endGroup."""
    start = block.index("beginGroup(EcIni::%s)" % group)
    end = block.index("endGroup()", start)
    return block[start:end]


def wind_filter_group(block):
    return ini_group(block, "INIGROUP_WIND_FILTER")


def planar_fit_group(block):
    return ini_group(block, "INIGROUP_SCREEN_TILT")


SAVE = "bool EcProject::saveEcProject(const QString &filename)"
LOAD = ("bool EcProject::loadEcProject(const QString &filename, "
        "bool checkVersion, bool *modified)")


def engine_tree():
    """The engine checkout beside this one, or beside the repository a
    worktree of this one belongs to."""
    for parent in ROOT.parents:
        candidate = parent / "eddyflow-engine" / "src"
        if candidate.is_dir():
            return candidate
    return None


class TheKeysAreTheEngines(unittest.TestCase):
    def test_the_prefix_is_the_engine_spelling(self):
        self.assertIn('INI_WIND_FILTER_PREFIX    = QStringLiteral("wdf_sect_")',
                      code(INIDEFS))
        self.assertIn('INIGROUP_WIND_FILTER      = '
                      'QStringLiteral("RawProcess_WindDirectionFilter")',
                      code(INIDEFS),
                      "ReadIniRP only sweeps RawProcess* sections")


class TheWriterCountsFromOne(unittest.TestCase):
    def setUp(self):
        self.group = wind_filter_group(
            body(PROJECT, "bool EcProject::saveEcProject(const QString &filename)"))

    def test_sector_i_is_written_as_i_plus_one(self):
        self.assertIn(
            "EcIni::INI_WIND_FILTER_PREFIX + QString::number(i + 1)",
            self.group)

    def test_nothing_is_written_at_zero(self):
        self.assertNotIn(
            "EcIni::INI_WIND_FILTER_PREFIX + QString::number(i) ",
            self.group,
            "wdf_sect_0_* matches no engine tag; that sector is never "
            "filtered")

    def test_every_old_sector_key_is_cleared_first(self):
        """A removed sector, or an old file's wdf_sect_0_*, must not survive
        a save for the reader to pick up again."""
        self.assertIn('QStringLiteral("^wdf_sect_\\\\d+_")', self.group)
        self.assertIn("project_ini.remove(key)", self.group)
        self.assertLess(
            self.group.index("project_ini.remove(key)"),
            self.group.index("INI_WIND_FILTER_START_SUFFIX"),
            "stale keys must be cleared before the new ones are written")


class OldProjectsStillLoadTheirSectors(unittest.TestCase):
    def setUp(self):
        self.load = body(
            PROJECT,
            "bool EcProject::loadEcProject(const QString &filename, "
            "bool checkVersion, bool *modified)")
        self.group = wind_filter_group(self.load)

    def test_the_reader_takes_any_index(self):
        """Not a loop from 1: that would drop an old file's first sector in
        the interface too, and the user would never see it go."""
        self.assertIn("project_ini.allKeys()", self.group)
        self.assertIn("key.startsWith(EcIni::INI_WIND_FILTER_PREFIX)",
                      self.group)
        self.assertIn("std::sort(orderedIndices.begin(), orderedIndices.end())",
                      self.group)

    def test_a_file_counted_from_zero_is_recognised(self):
        self.assertIn("windFilterZeroBased = sectorIndices.contains(0);",
                      self.group)

    def test_and_marked_modified_so_the_next_run_saves_it(self):
        """MainWindow::fileSaveSilently writes only a modified project, and the
        engine reads the file on disk, not the interface."""
        self.assertIn("if (windFilterZeroBased && modified != nullptr) "
                      "{ *modified = true; }",
                      self.load)

    def test_the_flag_is_set_after_the_modified_reset(self):
        """setModified(false) runs once loading is done; the flag must be
        reported after it, through the out-parameter, or it is lost."""
        self.assertLess(
            self.load.index("setModified(false);"),
            self.load.index("if (windFilterZeroBased && modified != nullptr)"))


class TheEngineCountsFromOne(unittest.TestCase):
    def setUp(self):
        engine = engine_tree()
        if engine is None:
            self.skipTest("engine tree not beside this one")
        self.engine = engine

    def test_the_tags_run_from_one_to_sixteen(self):
        table = self.engine / "src_rp" / "m_rp_global_var.f90"
        self.assertTrue(table.is_file(), "%s is gone" % table.name)
        text = table.read_text(encoding="utf-8", errors="replace")
        numbers = sorted({int(n) for n in
                          re.findall(r"'wdf_sect_(\d+)_(?:start|end)'", text)})
        self.assertEqual(list(range(1, 17)), numbers,
                         "the engine's sectors are numbered 1..16; the GUI "
                         "writer must follow whatever this says")

    def test_the_cap_matches_the_table(self):
        typedef = (self.engine / "src_common" / "m_typedef.f90").read_text(
            encoding="utf-8", errors="replace")
        m = re.search(r"MaxNumWdfSectors\s*=\s*(\d+)", typedef)
        self.assertIsNotNone(m, "MaxNumWdfSectors is gone")
        gui = re.search(r"MaxSectors\s*=\s*(\d+);", code(TABLE_H))
        self.assertIsNotNone(gui, "WindFilterTableModel::MaxSectors is gone")
        self.assertEqual(m.group(1), gui.group(1),
                         "a sector past the engine's cap is written and "
                         "never read")


class PlanarFitSectorsCountFromOneToo(unittest.TestCase):
    def test_the_prefix_and_group_are_the_engine_spelling(self):
        self.assertIn('INI_SCREEN_TILT_PREFIX = QStringLiteral("pf_sect_")',
                      code(INIDEFS))
        self.assertIn('INIGROUP_SCREEN_TILT   = '
                      'QStringLiteral("RawProcess_TiltCorrection_Settings")',
                      code(INIDEFS))

    def test_the_writer_counts_from_one(self):
        group = planar_fit_group(body(PROJECT, SAVE))
        self.assertIn('QStringLiteral("_") + QString::number(k + 1)', group)
        self.assertIn("insertIndex(EcIni::INI_SCREEN_TILT_PREFIX, 7, index)",
                      group,
                      "position 7 is after 'pf_sect', giving pf_sect_<k>_")

    def test_the_writer_empties_the_group_first(self):
        """Removing every key in the group is what keeps a removed sector
        from coming back."""
        group = planar_fit_group(body(PROJECT, SAVE))
        self.assertIn("project_ini.remove(QString());", group)
        self.assertLess(group.index("project_ini.remove(QString());"),
                        group.index("INI_SCREEN_TILT_PREFIX"))

    def test_the_reader_counts_from_one(self):
        group = planar_fit_group(body(PROJECT, LOAD))
        self.assertIn("EcIni::INI_SCREEN_TILT_PREFIX + QString::number(k + 1)",
                      group)

    def test_the_table_stops_at_the_engine_cap(self):
        self.assertIn("static constexpr int MaxSectors = 36;", code(PF_TABLE_H))
        insert = body(PF_TABLE, "bool AngleTableModel::insertRows(")
        self.assertIn("if (angles_->count() + count > MaxSectors) return false;",
                      insert)


class TheEngineReadsThirtySixPlanarFitSectors(unittest.TestCase):
    def setUp(self):
        engine = engine_tree()
        if engine is None:
            self.skipTest("engine tree not beside this one")
        self.engine = engine

    def test_the_tags_run_from_one_to_thirty_six(self):
        text = (self.engine / "src_rp" / "m_rp_global_var.f90").read_text(
            encoding="utf-8", errors="replace")
        numbers = sorted({int(n) for n in
                          re.findall(r"'pf_sect_(\d+)_(?:width|exclude)'",
                                     text)})
        self.assertEqual(list(range(1, 37)), numbers)

    def test_the_cap_matches_the_table(self):
        typedef = (self.engine / "src_common" / "m_typedef.f90").read_text(
            encoding="utf-8", errors="replace")
        m = re.search(r"MaxNumWSect\s*=\s*(\d+)", typedef)
        self.assertIsNotNone(m, "MaxNumWSect is gone")
        gui = re.search(r"MaxSectors\s*=\s*(\d+);", code(PF_TABLE_H))
        self.assertIsNotNone(gui, "AngleTableModel::MaxSectors is gone")
        self.assertEqual(m.group(1), gui.group(1))


if __name__ == "__main__":
    unittest.main()
