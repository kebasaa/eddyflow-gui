"""Guards the Instruments and Raw File Description tables against losing rows.

Both tables put their row labels in a `CustomHeader` - a widget of QLabels
sitting beside the table, not the table's own vertical header, which returns
empty strings. Nothing about that arrangement ties a label to a row, so it has
two failure modes and both had happened by the time anyone noticed:

  1. the container's height was a hard-coded number tuned for the row count of
     the day. The vertical scroll bar is off, so once the models gained
     Acquisition frequency and Sampling the last rows were simply unreachable -
     the analyser table lost two of them;
  2. the labels were left to size themselves, so QGridLayout shared the header's
     height out by each label's natural height. One label taller than a row -
     rich text such as k<sub>W</sub> renders taller - takes its extra from the
     rest and every label below it slides off its row, which is how the
     acquisition frequency came to read against "Time response".

Neither is visible to a unit test: the second is a pixel question and only the
eye settles it. What is checkable is that the mechanisms which prevent them are
still in place, and that the two lists which have to agree still do.
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


#: view stem -> (model header, enum name)
TABLES = {
    "anem_tableview": ("src/anem_model.h", "AnemItem"),
    "irga_tableview": ("src/irga_model.h", "IrgaItem"),
    "variable_tableview": ("src/variable_model.h", "VarItem"),
}


def enum_members(model_header, enum_name):
    body = read(model_header)
    m = re.search(r"enum\s+" + enum_name + r"\s*\{(.*?)\}", body, re.S)
    assert m, f"{enum_name} not found in {model_header}"
    names = [n.strip() for n in m.group(1).split(",")]
    return [n for n in names if n and not n.startswith("//")]


class TableRowsAreReachableTests(unittest.TestCase):
    def test_every_enum_row_has_a_label(self):
        """The two lists are written by hand in different files and only their
        order keeps the table honest."""
        for view, (model_header, enum_name) in TABLES.items():
            members = enum_members(model_header, enum_name)
            #> The last member is the NUMCOLS sentinel, not a row.
            rows = len(members) - 1
            sections = read(f"src/{view}.cpp").count("m_header->addSection")
            assert sections == rows, (
                f"{view} draws {sections} labels for {rows} rows of "
                f"{enum_name}: every row past the shorter of the two reads "
                f"against the wrong label"
            )

    def test_the_views_state_the_height_they_need(self):
        """With the vertical scroll bar off, a row that does not fit is a row
        nobody can reach."""
        for view in TABLES:
            body = read(f"src/{view}.cpp")
            assert "::minimumSizeHint() const" in body, (
                f"{view} no longer states a height, so its container decides "
                f"how many rows exist and the rest are unreachable"
            )
            assert "model()->rowCount()" in body, (
                f"{view}'s height is no longer derived from the row count"
            )

    def test_no_container_pins_these_tables_to_a_literal_height(self):
        for f in ("src/dlinstrtab.cpp", "src/dlrawfiledesctab.cpp"):
            body = read(f)
            hits = re.findall(r"setMinimumHeight\(\s*\d+\s*\)", body)
            assert not hits, (
                f"{f} fixes a table's height to a literal again ({hits}); that "
                f"number is right until the next row is added"
            )

    def test_each_header_section_is_pinned_to_one_row(self):
        head = read("src/customheader.h")
        assert "setSectionHeight" in head, (
            "CustomHeader no longer pins its sections, so the labels drift "
            "off their rows wherever one of them renders taller than a row"
        )
        body = read("src/customheader.cpp")
        assert "setRowMinimumHeight" in body and "setFixedHeight" in body, (
            "the section height is declared but not applied to the labels"
        )
        for view in TABLES:
            assert "setSectionHeight(rowH)" in read(f"src/{view}.cpp"), (
                f"{view} never tells its header the table's row height"
            )

    def test_the_header_starts_where_the_rows_start(self):
        """It used to start half a row above the viewport, which offset every
        label by half a row before any drift was added to it."""
        for view in TABLES:
            body = read(f"src/{view}.cpp")
            assert "rowHeight(0) / 2.0" not in body, (
                f"{view} places the labels half a row above row 0 again"
            )


if __name__ == "__main__":
    unittest.main()
