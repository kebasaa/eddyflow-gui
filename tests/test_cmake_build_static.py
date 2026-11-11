"""The CMake build compiles what is there and puts things where the code looks.

CMakeLists.txt replaced three qmake project files. These checks hold the few
places where the build and the code have to agree, and that only a run of the
built program, not a compile, would otherwise catch:

- every source it lists exists, so a renamed file fails here rather than at
  configure time on someone else's machine;
- the translation is embedded under the path main.cpp loads, :/tra/en;
- a release build defines QT_NO_DEBUG_OUTPUT, which test_help_links_static.py
  relies on, and the debug build keeps a console;
- the binary keeps the names and folders eddyflow-build-script looks for.
"""
import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
CMAKELISTS = GUI_ROOT / "CMakeLists.txt"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


BUILD = read(CMAKELISTS)


class TheListedSourcesExist(unittest.TestCase):

    def test_every_listed_file_is_there(self):
        listed = re.findall(r"(?m)^\s+(src/\S+\.(?:h|cpp))\s*$", BUILD)
        self.assertGreater(len(listed), 200, "the source lists were not found")
        missing = [f for f in listed if not (GUI_ROOT / f).is_file()]
        self.assertEqual(missing, [])

    def test_no_file_is_listed_twice(self):
        listed = re.findall(r"(?m)^\s+(src/\S+\.(?:h|cpp))\s*$", BUILD)
        self.assertEqual(len(listed), len(set(listed)))

    def test_no_qmake_project_is_left_to_drift(self):
        for name in ("EddyFlow_win.pro", "EddyFlow_lin.pro", "EddyFlow_mac.pro",
                     "sources.pri", "libs.pri"):
            self.assertFalse((GUI_ROOT / name).exists(), name)


class TheTranslationIsWhereMainLoadsIt(unittest.TestCase):

    def test_main_loads_tra_en(self):
        self.assertIn('appTranslator.load(QStringLiteral(":/tra/en"))',
                      read(GUI_ROOT / "src" / "main.cpp"))

    def test_the_build_embeds_it_there(self):
        self.assertIn("TS_FILES tra/EddyFlow_en.ts", BUILD)
        self.assertIn("PROPERTIES QT_RESOURCE_ALIAS en", BUILD)
        self.assertIn('PREFIX "/tra"', BUILD)

    def test_no_qrc_still_wants_a_prebuilt_qm(self):
        for name in ("EddyFlow_win.qrc", "EddyFlow_lin.qrc", "EddyFlow_mac.qrc"):
            self.assertNotIn("EddyFlow_en.qm", read(GUI_ROOT / name), name)


class DebugAndReleaseDiffer(unittest.TestCase):

    def test_release_silences_qdebug(self):
        self.assertRegex(
            BUILD,
            r"\$<\$<NOT:\$<CONFIG:Debug>>:QT_NO_DEBUG QT_NO_DEBUG_OUTPUT QT_NO_WARNING_OUTPUT>")

    def test_the_debug_build_keeps_its_console(self):
        start = BUILD.index("WIN32_EXECUTABLE TRUE")
        self.assertIn('if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")',
                      BUILD[start - 200:start])


class TheBinaryIsWhereTheScriptsLook(unittest.TestCase):

    def test_names_and_folders(self):
        self.assertIn("qt_add_executable(EddyFlow ", BUILD)
        self.assertIn("OUTPUT_NAME_DEBUG EddyFlow_debug", BUILD)
        self.assertIn('RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<LOWER_CASE:$<CONFIG>>"',
                      BUILD)


if __name__ == "__main__":
    unittest.main()
