"""A malformed instrument label is refused, not indexed into.

``DlProject::toIniVariableInstrument`` turns the label a variable's row shows -
``"Sonic 1: HS-50"`` - into the id the metadata file stores, ``hs_50_1``. It
used to take the pieces straight out of two splits:

    QString numberStr = s.split(':').at(0);
    QString modelStr  = s.split(':').at(1);      // no colon -> out of range
    QString instrType = numberStr.split(' ').at(0);
    QString number    = numberStr.split(' ').at(1);   // no space -> ditto

so a label of any other shape ran off the end of a QStringList and took the
application down. **This is called from saveProject**, so it was not an exotic
path: a hand-edited metadata file, or one written by anything that did not know
the exact form, crashed the save rather than being refused. It was hit for real
while writing ``src/eddyuhimport.cpp``, which builds labels programmatically.

Confirmed by building the current probe against the code as it stood before the
guards: the four well-formed cases converted, and the fifth - ``"hs_50_1"``,
an id passed where a label was expected - exited 0xC0000005.

``fromIniVariableInstrument`` carries the same assumption in the other
direction and is guarded the same way. That one never crashed:
``QString::left(-1)`` hands back the whole string rather than throwing, so with
no underscore to split on it put the model in both halves and built labels like
``"Sonic li7200: LI-7200"``. Wrong rather than fatal, and now refused.

What must keep working, and is asserted below as well as the refusals:

* the two well-formed shapes, ``Sonic``/``Irga``;
* ``"Other"``, which is a valid label and not a malformed one;
* an empty label, which already meant "no instrument";
* ``_1`` - an instrument whose model has not been chosen yet writes that, and
  it has always read back that way. Only a *missing* underscore is refused,
  never one at position zero, or the round trip would lose a real state.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
DLPROJECT = GUI_ROOT / "src" / "dlproject.cpp"

SRC = DLPROJECT.read_text(encoding="utf-8", errors="replace")


def body(name):
    """The body of one conversion function."""
    at = SRC.index("QString DlProject::%s(const QString& s)" % name)
    end = SRC.index("\nQString DlProject::", at + 10)
    return SRC[at:end]


TO_INI = body("toIniVariableInstrument")
FROM_INI = body("fromIniVariableInstrument")


class TheSplitsAreBoundsChecked(unittest.TestCase):
    """The crash itself."""

    def test_no_element_is_taken_before_the_counts_are_known(self):
        #> Every read of a split result is either .value(), which returns a
        #> default when out of range, or sits after the guard.
        for m in re.finditer(r"\.at\((\d+)\)", TO_INI):
            self.fail("toIniVariableInstrument still indexes with .at(%s); "
                      "use .value() or check the size first" % m.group(1))

    def test_both_counts_are_checked(self):
        self.assertIn("halves.size() < 2", TO_INI)
        self.assertIn("head.size() < 2", TO_INI)

    def test_the_check_precedes_every_use(self):
        guard = TO_INI.index("halves.size() < 2")
        for use in ("instrType == tr(\"Sonic\")", "toIniAnemModel(",
                    "toIniIrgaModel(", "QLatin1Char('_')"):
            self.assertGreater(TO_INI.index(use), guard,
                               "%s is reached before the guard" % use)

    def test_a_malformed_label_converts_to_nothing(self):
        #> The same thing an empty label already produced, and what the file
        #> means by a column with no instrument.
        block = TO_INI[TO_INI.index("halves.size() < 2"):]
        block = block[:block.index("}")]
        self.assertIn("return QString();", block)

    def test_an_empty_number_is_refused_too(self):
        #> " : " splits into two halves and two words, passing a count check,
        #> and then builds the bare "_" - no more a usable id than a crash.
        self.assertIn("number.isEmpty()", TO_INI)


class TheReverseDirectionIsGuarded(unittest.TestCase):
    """It garbled rather than crashed, which is harder to notice."""

    def test_a_missing_underscore_is_refused(self):
        self.assertIn("index < 0", FROM_INI)
        block = FROM_INI[FROM_INI.index("index < 0"):]
        self.assertIn("return QString();", block[:block.index("}") + 1])

    def test_the_guard_precedes_the_substrings(self):
        self.assertLess(FROM_INI.index("index < 0"),
                        FROM_INI.index("s.mid(index + 1)"))
        self.assertLess(FROM_INI.index("index < 0"),
                        FROM_INI.index("s.left(index)"))

    def test_an_underscore_at_position_zero_still_works(self):
        #> "_1" is an instrument with no model chosen. Refusing index <= 0
        #> would drop a state the interface can genuinely be in, and the
        #> round trip through toIniVariableInstrument would stop closing.
        self.assertNotIn("index <= 0", FROM_INI)


class TheWellFormedShapesStillConvert(unittest.TestCase):
    """A guard that refused everything would also pass the tests above."""

    def test_both_instrument_types_are_still_dispatched(self):
        self.assertIn("toIniAnemModel(", TO_INI)
        self.assertIn("toIniIrgaModel(", TO_INI)
        self.assertIn('tr("Sonic")', TO_INI)

    def test_the_id_is_still_model_underscore_number(self):
        self.assertIn("model + QLatin1Char('_') + number", TO_INI)

    def test_other_is_still_a_valid_label_and_not_a_malformed_one(self):
        #> It has no colon, so a guard placed before the special case would
        #> turn the one label that is deliberately shapeless into nothing.
        self.assertLess(TO_INI.index('QLatin1String("Other")'),
                        TO_INI.index("halves.size() < 2"))
        self.assertIn('QStringLiteral("other")', TO_INI)

    def test_an_empty_label_is_still_empty_rather_than_refused(self):
        self.assertIn("if (!s.isEmpty())", TO_INI)

    def test_the_reverse_still_builds_both_labels(self):
        self.assertIn('tr("Sonic ")', FROM_INI)
        self.assertIn('tr("Irga ")', FROM_INI)
        self.assertIn('tr("Other")', FROM_INI)


class TheReasonIsRecordedWhereItWillBeRead(unittest.TestCase):

    def test_the_source_says_what_it_is_guarding_against(self):
        #> The next person to read this will want to know the guard is load
        #> bearing and not defensive habit.
        self.assertIn("saveProject", TO_INI)
        self.assertIn("eddyuhimport.cpp", TO_INI)

    def test_it_records_that_the_reverse_garbled_rather_than_crashed(self):
        self.assertIn("left(-1)", FROM_INI)


if __name__ == "__main__":
    unittest.main()
