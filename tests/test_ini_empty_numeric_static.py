"""An empty numeric key reads as its default, not as zero.

QSettings::value(key, fallback) hands back the fallback only when the key is
ABSENT. A line written as `error_value=` is a key that EXISTS holding an empty
string, so the fallback is skipped and QVariant::toReal() answers 0.0 - it has
no "did it convert?" flag, unlike QString::toDouble(). A metadata file written
elsewhere with an empty error_value therefore declared 0 to be its missing-data
fill, and every genuine zero in that column was read as missing. The same shape
carried every other numeric key: an empty acquisition_frequency read as 0 Hz,
an empty path length as 0 m.

The fix is a type, not a rule to remember: the readers declare their file as an
IniFile, whose value() keeps hold of the fallback so the conversion can use it.
Every existing read is unchanged, and a read added later is fixed by
construction. That is also what makes it fragile in one specific way - declare
the file a plain QSettings again and the old reading is silently back - so the
declaration is what this pins.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent

INI_FILE_H = GUI_ROOT / "src" / "ini_file.h"
INI_FILE_CPP = GUI_ROOT / "src" / "ini_file.cpp"
READERS = {
    "metadata": GUI_ROOT / "src" / "dlproject.cpp",
    "project": GUI_ROOT / "src" / "ecproject.cpp",
}


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


class TheTypeHoldsOnToTheFallback(unittest.TestCase):
    def test_value_answers_a_scalar_that_knows_the_fallback(self):
        header = read(INI_FILE_H)
        self.assertIn("class IniFile : public QSettings", header)
        self.assertIn("IniScalar value(const QString& key,", header)
        self.assertIn("const QVariant& fallback = QVariant()) const;", header)

    def test_absent_empty_and_unparseable_all_reach_the_fallback(self):
        body = read(INI_FILE_CPP)

        # absent: QSettings answers an invalid QVariant
        self.assertIn("if (!value_.isValid()) { return QString(); }", body)
        # present but blank, whitespace included
        self.assertIn("return value_.toString().trimmed();", body)
        # every numeric conversion asks first, and falls back when it cannot read
        for conversion in ("toInt", "toDouble"):
            self.assertRegex(
                body,
                r"%s\(\) const\s*\{\s*const auto number = text\(\);\s*"
                r"if \(number\.isEmpty\(\)\) \{ return fallback_\.%s\(\); \}" % (conversion, conversion))
        # the &ok form, which QVariant has and QSettings::value does not expose
        self.assertIn("number.toInt(&ok)", body)
        self.assertIn("number.toDouble(&ok)", body)

    def test_a_string_keeps_its_old_reading(self):
        # an empty string is a legitimate value, unlike an empty number
        body = read(INI_FILE_CPP)
        self.assertIn(
            "return value_.isValid() ? value_.toString() : fallback_.toString();",
            body)


class TheReadersUseIt(unittest.TestCase):
    def test_both_readers_declare_their_file_as_an_ini_file(self):
        for name, path in READERS.items():
            with self.subTest(reader=name):
                body = read(path)
                self.assertRegex(
                    body, r"IniFile project_ini\([a-zA-Z]+, QSettings::IniFormat\);",
                    "%s must read through IniFile, or an empty numeric key is 0 again"
                    % path.name)
                self.assertIn('#include "ini_file.h"', body)

    def test_the_loader_is_the_one_that_opens_an_ini_file(self):
        #: loader function -> the writer that follows it, so the slice is the
        #: loader alone; only the loader reads, and only the loader must change
        loaders = {
            "metadata": ("DlProject::loadProject", "DlProject::saveProject"),
            "project": ("EcProject::loadEcProject", "EcProject::compactGasRecords"),
        }
        for name, path in READERS.items():
            with self.subTest(reader=name):
                body = read(path)
                start, end = loaders[name]
                self.assertIn(start, body)
                self.assertIn(end, body)
                loader = body[body.index(start):body.index(end, body.index(start))]
                self.assertIn("IniFile project_ini(", loader)
                self.assertNotIn("QSettings project_ini(", loader)

    def test_the_reported_case_still_states_its_default(self):
        body = read(READERS["metadata"])
        self.assertIn("DlIni::INI_VARDESC_ERROR_VALUE, -9999.0", body)

    def test_the_enum_keys_convert_through_the_scalar(self):
        # these three read into a QVariant first, which would hold "" for an
        # empty key and convert to 0 - a file type and a run mode in their own
        # right - so they ask the scalar for the number instead
        body = read(READERS["project"])
        self.assertNotIn("QVariant v; // container for conversions", body)
        for const in ("INI_PROJECT_7", "INI_PROJECT_33_OLD", "INI_PROJECT_33"):
            self.assertRegex(
                body,
                r"value\(EcIni::%s,\s*\n?\s*QVariant::fromValue\([^;]*?\)\.toInt\(\)\)\.toInt\(\)" % const,
                "%s must convert through IniScalar::toInt" % const)


if __name__ == "__main__":
    unittest.main()
