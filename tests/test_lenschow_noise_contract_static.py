"""The Lenschow noise method agrees across the interface and the engine.

No new ini key: ``ru_meth`` already carried the method number, and this is one
more value on it. What has to agree is the **mapping**, and it is the mapping
that is easy to get wrong here - the combo row and the method number have not
run in step since 3 became Mahrt, so the fourth row stores 5.

The other thing worth pinning is the tooltip. Four entries in one menu, all
producing a number in the same units, three of which answer different
questions. The menu itself cannot show that; only the text can.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_DECODER = (ENGINE_ROOT / "src" / "src_common"
                  / "write_processing_project_variables.f90")


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


PAGE = read(GUI_ROOT / "src" / "advstatisticaloptions.cpp")

#: combo row -> stored value -> the engine's method name.
ROWS = {
    0: (1, "finkelstein_sims_01"),
    1: (2, "mann_lenschow_94"),
    2: (4, "billesbach_11"),
    3: (5, "lenschow_00"),
}


class TheRowIsNotTheMethodNumber(unittest.TestCase):

    def test_every_row_stores_the_value_it_should(self):
        for row, (value, _) in ROWS.items():
            self.assertIn(
                "randomMethodCombo->setItemData(%d, %d);" % (row, value), PAGE,
                "combo row %d does not store ru_meth %d" % (row, value))

    def test_the_menu_has_exactly_those_four_rows(self):
        block = PAGE[PAGE.index("randomMethodCombo->addItem("):]
        block = block[:block.index("randomMethodCombo->setItemData(0,")]
        self.assertEqual(block.count("randomMethodCombo->addItem("), len(ROWS))

    def test_nothing_derives_the_value_from_the_row(self):
        #> `method - 1` held only while the two ran in step. Four sites did
        #> that arithmetic before Billesbach; every one is a findData or an
        #> itemData call now, and a fifth entry must not reintroduce it.
        self.assertNotIn("randomMethodCombo->currentIndex() + 1", PAGE)
        self.assertNotIn("randomMethodCombo->setCurrentIndex(method - 1)", PAGE)
        self.assertIn("randomMethodCombo->currentData().toInt()", PAGE)

    @unittest.skipUnless(ENGINE_DECODER.is_file(), "engine repo not beside the GUI")
    def test_each_stored_value_reaches_the_method_it_names(self):
        src = read(ENGINE_DECODER)
        for row, (value, name) in ROWS.items():
            self.assertRegex(
                src, r"case\(%d\)[\s\S]{0,400}?RUsetup%%meth = '%s'"
                % (value, name),
                "combo row %d stores %d, which the engine does not decode to %s"
                % (row, value, name))


class TheTooltipSaysWhichQuestionItAnswers(unittest.TestCase):

    def tip(self, row):
        m = re.search(
            r"randomMethodCombo->setItemData\(%d, tr\((.*?)\), Qt::ToolTipRole\);"
            % row, PAGE, re.S)
        self.assertIsNotNone(m, "row %d has no tooltip" % row)
        return "".join(re.findall(r'"([^"]*)"', m.group(1)))

    def test_it_says_this_is_the_instrument_and_not_the_sampling(self):
        tip = self.tip(3)
        self.assertIn("analyser's own noise", tip)
        self.assertIn("not interchangeable", tip)

    def test_it_says_it_declines_periods(self):
        #> A user who sees mostly -9999 needs to know that is the method
        #> working, not the run failing.
        tip = self.tip(3)
        self.assertIn("declines periods", tip)
        self.assertIn("reported as missing", tip)

    def test_it_warns_that_the_window_is_in_samples(self):
        self.assertIn("fixed in samples", self.tip(3))

    def test_billesbach_still_says_it_is_a_floor_not_an_error(self):
        #> The two smallest estimates sit next to each other now. If either
        #> stops saying what it is, they read as alternatives.
        self.assertIn("noise floor, not a sampling error", self.tip(2))


if __name__ == "__main__":
    unittest.main()
