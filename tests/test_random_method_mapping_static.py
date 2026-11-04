"""The random-uncertainty menu maps to the engine's ru_meth values by data.

The menu used to be read as ``currentIndex() + 1``. That held only while it
listed exactly the first two methods, because the engine's enumeration happens
to start at one and run in the same order. It stopped holding the moment a
third entry was added: the engine's 3 is Mahrt (1998), which this menu does
not offer because the engine computes it unconditionally, so the third row is
Billesbach at 4 and its position says nothing about its value.

An index-based mapping that survives here does not fail - it silently selects
a different method than the one named on the row, which then runs and
publishes numbers under the wrong heading.

So: every row carries its engine value as item data, every read goes through
that data, and nothing does arithmetic on the row number.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_PRJVARS = (ENGINE_ROOT / "src" / "src_common"
                  / "write_processing_project_variables.f90")

PAGE = (GUI_ROOT / "src" / "advstatisticaloptions.cpp").read_text(
    encoding="utf-8", errors="replace")

#: row -> (engine ru_meth value, a word from the label)
ROWS = {
    0: (1, "Finkelstein"),
    1: (2, "Mann"),
    2: (4, "Billesbach"),
}


class TheMenuCarriesEngineValues(unittest.TestCase):

    def test_every_row_declares_its_value_as_item_data(self):
        for row, (value, _) in ROWS.items():
            self.assertRegex(
                PAGE,
                r"randomMethodCombo->setItemData\(%d,\s*%d\)" % (row, value),
                "row %d does not carry engine value %d as item data"
                % (row, value),
            )

    def test_every_row_is_labelled(self):
        for row, (_, word) in ROWS.items():
            self.assertRegex(
                PAGE,
                r'randomMethodCombo->addItem\(tr\("[^"]*%s' % word,
                "row %d is no longer the %s entry" % (row, word),
            )

    def test_nothing_derives_the_method_from_the_row_number(self):
        #> The three sites that used to: updateRandomErrorArea,
        #> updateRandomMethod and both refresh paths.
        offenders = re.findall(
            r"setRandomErrorMethod\([^)]*(?:currentIndex\(\)|\bn\b)\s*\+\s*1[^)]*\)",
            PAGE)
        self.assertEqual(
            offenders, [],
            "the method is being derived from the row number again: %s"
            % offenders,
        )

    def test_nothing_derives_the_row_number_from_the_method(self):
        offenders = re.findall(
            r"randomMethodCombo->setCurrentIndex\(\s*\w*[Rr]andom\w*\s*-\s*1\s*\)",
            PAGE)
        self.assertEqual(
            offenders, [],
            "the row is being derived from the method value again: %s"
            % offenders,
        )

    def test_the_reverse_lookup_goes_through_find_data(self):
        self.assertGreaterEqual(
            PAGE.count("randomMethodCombo->findData("), 2,
            "both refresh paths should locate the row by its stored value",
        )

    def test_a_method_the_menu_does_not_offer_leaves_the_row_alone(self):
        #> findData returns -1 for ru_meth = 3. Setting that as an index would
        #> clear the combo; guarding it keeps whatever was showing.
        self.assertEqual(
            PAGE.count("if (row >= 0) { randomMethodCombo->setCurrentIndex(row); }"),
            2,
            "a findData miss is no longer guarded, so a project holding "
            "ru_meth = 3 would blank or mis-set the menu",
        )


@unittest.skipUnless(ENGINE_PRJVARS.is_file(),
                     "engine repo not checked out beside the GUI")
class TheValuesMatchTheEngine(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.engine = ENGINE_PRJVARS.read_text(encoding="utf-8", errors="replace")

    def test_the_engine_decodes_each_value_to_the_named_method(self):
        expected = {
            1: "finkelstein_sims_01",
            2: "mann_lenschow_94",
            4: "billesbach_11",
        }
        for value, meth in expected.items():
            self.assertRegex(
                self.engine,
                r"case\(%d\)\s*\n(?:\s*!>[^\n]*\n)*\s*RUsetup%%meth = '%s'"
                % (value, meth),
                "the engine no longer decodes ru_meth = %d as %r; the menu "
                "would select something else" % (value, meth),
            )

    def test_three_is_still_mahrt_and_still_absent_from_the_menu(self):
        self.assertRegex(
            self.engine,
            r"case\(3\)\s*\n\s*RUsetup%meth = 'mahrt_98'",
            "ru_meth = 3 changed meaning in the engine",
        )
        self.assertNotIn(
            "setItemData(2, 3)", PAGE,
            "the menu now claims row 2 is ru_meth 3, which is Mahrt, not "
            "Billesbach",
        )


if __name__ == "__main__":
    unittest.main()
