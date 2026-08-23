"""The EddyUH project importer, and the five things that make it honest.

An EddyUH project is four MATLAB-and-text files sharing a stem. The user picks
``preproc_<stem>``; the rest are found beside it and each is optional.

1. **Nothing is written until the user saves.** Same rule as the EddyPro
   import, and for the same reason: opening a project to look at it is not
   consent to write two files over it.
2. **Every guess is reported.** An EddyUH project is missing a great deal that
   EddyFlow needs, and its flux-time options are not in the files *at all* -
   they are collected interactively at every run and written only to a text log
   beside the fluxes. An import that succeeded silently would be claiming to
   have carried settings it never saw.
3. **No index is ever used as a vocabulary lookup.** Every ``...StringList()``
   in this application is sorted alphabetically before it is returned, so the
   position of a string in the list is not the number of the accessor that
   produced it. The first version of this importer indexed the lists and turned
   raw column 1 into AGC and a Gill HS into an 81000RE - silently, in a file
   that looked plausible.
4. **The lag windows are seconds.** ``EddyUH_SC_Preproc.m:84`` computes
   ``round(fs*(lags-dlags))``, so ``lags`` is in seconds for every analyser -
   whatever integer class MATLAB happened to store it in. One project holds
   ``uint8`` for one instrument and ``float64`` for the next.
5. **An unknown name is not guessed at.** EddyUH's instrument names are free
   text typed into its setup dialog, so a miss is expected; what must not
   happen is a plausible substitution nobody is told about.

Also pinned: the instrument label a column carries must be the "Sonic 1: HS-50"
form. ``DlProject::toIniVariableInstrument`` splits it on a colon and then on a
space and takes element 1 of each without checking, so a bare model name does
not produce a wrong file - it crashes the save.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
IMPORT_H = GUI_ROOT / "src" / "eddyuhimport.h"
IMPORT_CPP = GUI_ROOT / "src" / "eddyuhimport.cpp"
MAINWINDOW = GUI_ROOT / "src" / "mainwindow.cpp"
MAINWINDOW_H = GUI_ROOT / "src" / "mainwindow.h"
SOURCES_PRI = GUI_ROOT / "sources.pri"

EDDYUH_SRC = (GUI_ROOT.parent / "EddyUH_testing" / "EddyUH" / "EddyUH_1.7b_COS")
SC_PREPROC = EDDYUH_SRC / "EC_Software_Preproc" / "EddyUH_SC_Preproc.m"
READ_RAW = EDDYUH_SRC / "EC_Software_Common" / "EddyUH_ReadFileRaw.m"
COORDROT = EDDYUH_SRC / "EC_Software_Common" / "EddyUH_coordrot.m"
COVAR = EDDYUH_SRC / "Functions_Library" / "COVAR.m"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


HDR = read(IMPORT_H)
SRC = read(IMPORT_CPP)
MW = read(MAINWINDOW)
UH_FN = MW[MW.index("void MainWindow::importEddyUhFile"):]


class NothingIsWrittenUntilTheUserSaves(unittest.TestCase):

    def test_the_importer_opens_no_file_for_writing(self):
        for forbidden in ("QIODevice::WriteOnly", "QIODevice::Append",
                          "saveEcProject", "saveProject("):
            self.assertNotIn(forbidden, SRC, forbidden)

    def test_the_deferred_save_is_begun_before_the_conversion(self):
        #> DlProject marks itself modified as it is filled, and modified is
        #> what would write it. After the conversion is too late.
        self.assertIn("beginDeferredSave()", UH_FN)
        self.assertLess(UH_FN.index("beginDeferredSave()"),
                        UH_FN.index("importer.convert("))

    def test_the_document_is_left_unsaved(self):
        head = UH_FN[:UH_FN.index("WidgetUtils::information")]
        self.assertIn("newFlag_ = true;", head)
        self.assertIn("const bool asModified = true;", head)

    def test_the_dialog_says_nothing_has_been_written(self):
        self.assertIn("Nothing has been written yet", UH_FN)


class EveryGuessIsReported(unittest.TestCase):

    def test_the_notes_are_part_of_the_interface(self):
        self.assertIn("QStringList notes() const", HDR)

    def test_the_flux_time_options_are_named_as_unrecoverable(self):
        #> The single most important thing an EddyUH user must be told.
        self.assertIn("collects its flux-time options afresh at every", SRC)
        for missing in ("spectral correction method", "cospectral model",
                        "peak-frequency", "time-lag method", "data screening",
                        "footprint"):
            self.assertIn(missing, SRC, missing)

    def test_that_note_is_unconditional(self):
        #> Not inside a branch: it is true of every EddyUH project ever
        #> written, so an import with an empty note list would be a lie.
        #>
        #> Pinned on the indentation of the whole line, not on what precedes
        #> the note( token: a guard written on the same line sits BEFORE it,
        #> and a slice starting at note( steps right over one. Found by
        #> injecting exactly that and watching this check not bite.
        at = SRC.index("collects its flux-time options afresh")
        nl = chr(10)
        line_start = SRC.rindex(nl, 0, at) + 1
        line = SRC[line_start:SRC.index(nl, at)]
        self.assertTrue(line.startswith("    note(QObject::tr("),
                        "the note must stand at the top level of convert(), "
                        "not behind a condition: %r" % line[:60])

    def test_longitude_is_reported_as_absent(self):
        self.assertIn("Longitude is not in an EddyUH project", SRC)

    def test_the_north_alignment_is_reported_as_assumed(self):
        #> Not in EddyUH at all, and wrong by thirty degrees turns every wind
        #> direction. Assumed rather than left blank, because a metadata file
        #> with no wref is not valid for a Gill.
        self.assertIn("does not record the anemometer's north", SRC)
        self.assertIn("getANEM_NORTH_ALIGN_STRING_0()", SRC)

    def test_the_unsigned_separation_is_reported(self):
        #> EddyUH has one scalar and no direction; EddyFlow has a signed pair.
        self.assertIn("which records no direction", SRC)
        self.assertIn("setTubeNSeparation", SRC)
        self.assertIn("setTubeESeparation(0.0)", SRC)

    def test_the_analyser_path_length_is_deliberately_not_imported(self):
        #> EddyUH stores one and uses it nowhere, so its value is unchecked
        #> free text - and EddyFlow's path averaging does use it.
        self.assertIn("was NOT imported", SRC)
        self.assertNotIn("irga.setHPathLength", SRC)
        self.assertNotIn("irga.setVPathLength", SRC)

    def test_the_dialog_shows_the_notes(self):
        self.assertIn("importer.notes()", UH_FN)
        self.assertIn("What you must still set", UH_FN)


class NoIndexIsUsedAsAVocabularyLookup(unittest.TestCase):
    """The bug that produced a plausible, wrong file."""

    def test_the_sorted_lists_are_never_indexed(self):
        for listed in ("variableStringList", "inputUnitStringList",
                       "measureTypeStringList", "allModelStringList",
                       "manufacturerStringList"):
            self.assertNotIn(listed, SRC,
                             "%s is sorted; indexing it is how a Gill HS "
                             "became an 81000RE" % listed)

    def test_the_accessors_are_called_directly(self):
        for accessor in ("VariableDesc::getVARIABLE_VAR_STRING_0()",
                         "AnemDesc::getANEM_MODEL_STRING_1()",
                         "IrgaDesc::getIRGA_MODEL_STRING_4()"):
            self.assertIn(accessor, SRC, accessor)

    def test_the_reason_is_recorded_where_it_will_be_read(self):
        self.assertIn("sorted alphabetically", SRC)
        self.assertIn("81000RE", SRC)


class TheLagWindowsAreSeconds(unittest.TestCase):

    def test_no_conversion_by_the_sampling_rate(self):
        #> Dividing by fs is exactly the mistake the note warns against.
        block = SRC[SRC.index('src.field(QStringLiteral("lags")'):]
        block = block[:block.index("dl->variables()->append")]
        self.assertNotIn("/ fs", block)
        self.assertNotIn("fs *", block)

    def test_the_window_is_the_nominal_plus_and_minus_the_margin(self):
        self.assertIn("var.setNomTimelag(nominal);", SRC)
        self.assertIn("var.setMinTimelag(nominal - margin);", SRC)
        self.assertIn("var.setMaxTimelag(nominal + margin);", SRC)

    def test_the_source_says_why_the_stored_class_means_nothing(self):
        self.assertIn("SECONDS for every analyser", SRC)

    @unittest.skipUnless(SC_PREPROC.is_file(), "EddyUH sources not present")
    def test_eddyuh_agrees(self):
        src = read(SC_PREPROC)
        self.assertIn("lags = middle of lag time window (in sec)", src)
        self.assertIn("round(fs*(lags(jj)-dlags(jj)))", src)


class UnknownNamesAreNotGuessedAt(unittest.TestCase):

    def test_an_unrecognised_anemometer_leaves_the_model_unset(self):
        block = SRC[SRC.index("lookupModel(kAnemometers"):]
        block = block[:block.index("anem.setId(")]
        self.assertIn("else", block)
        self.assertIn("not a name EddyFlow knows", block)

    def test_an_unrecognised_variable_becomes_ignore_and_says_so(self):
        self.assertIn("not a variable EddyFlow", SRC)
        self.assertIn("It was set to Ignore", SRC)

    def test_an_unclaimed_raw_column_is_ignored_rather_than_dropped(self):
        #> Dropping it would shift every column after it by one.
        self.assertIn("Not claimed by any instrument", SRC)
        self.assertIn("still lines up with the file", SRC)

    def test_a_name_that_is_matched_only_approximately_is_flagged(self):
        self.assertIn("kApproximateAnalysers", SRC)
        #> The literal is split across source lines, so the assertion is on
        #> a fragment rather than on the sentence a reader sees.
        self.assertIn("names a manufacturer but not a ", SRC)

    def test_a_one_dimensional_rotation_is_refused_not_promoted(self):
        #> EddyFlow has no yaw-only rotation, and a double rotation would
        #> additionally null the vertical wind - which the project chose not
        #> to do.
        #>
        #> Anchored on the rotation switch, not on "case 1:": the vocabulary
        #> functions above are switches too and theirs comes first.
        block = SRC[SRC.index("const int rot = pre.value("):]
        block = block[block.index("case 1:"):]
        block = block[:block.index("case 2:")]
        self.assertIn("setScreenRotMethod(0)", block)
        self.assertIn("rather than promoted", block)


class TheInstrumentLabelIsTheFormTheWriterParses(unittest.TestCase):

    def label_body(self):
        block = SRC[SRC.index("auto instrumentLabel"):]
        return block[:block.index("\n    };")]

    def test_the_label_carries_a_type_a_number_and_a_model(self):
        block = self.label_body()
        self.assertIn('QObject::tr("Sonic")', block)
        self.assertIn('QObject::tr("Irga")', block)
        self.assertIn('QStringLiteral(": ")', block)

    def test_an_unknown_model_becomes_other_rather_than_an_empty_label(self):
        #> toIniVariableInstrument handles "Other" as a special case and
        #> crashes on anything without a colon.
        self.assertEqual(self.label_body().count('QStringLiteral("Other")'), 3)

    def test_the_hazard_is_recorded(self):
        self.assertIn("crashes the save", SRC)


class TheSiblingsAreOptional(unittest.TestCase):

    def test_they_are_matched_on_the_stem_not_a_full_name(self):
        #> lag_<stem>.10cl, planar_fit_<stem>.1cl - the suffix is a run
        #> counter that changes every time EddyUH writes one.
        block = SRC[SRC.index("QStringList EddyUhImport::siblingsOf"):]
        block = block[:block.index("\n}")]
        for prefix in ("lag_", "planar_fit_", "resptime_"):
            self.assertIn('QStringLiteral("%s")' % prefix, block)
        self.assertIn('QStringLiteral("*")', block)

    def test_the_confirmation_lists_what_was_found(self):
        self.assertIn("siblingsOf(fileStr)", UH_FN)
        self.assertIn("Found beside it", UH_FN)

    def test_a_project_without_them_still_converts(self):
        #> convert() takes only the preproc path and never requires a sibling.
        sig = re.search(r"bool EddyUhImport::convert\((.*?)\)", SRC, re.S)
        self.assertIsNotNone(sig)
        self.assertNotIn("lagPath", sig.group(1))


class ThePreprocFileIsIdentifiedByContentNotExtension(unittest.TestCase):
    """It has no extension at all."""

    def test_the_check_reads_the_mat_header(self):
        block = SRC[SRC.index("bool EddyUhImport::looksLikeEddyUhProject"):]
        block = block[:block.index("\n}")]
        self.assertIn('QByteArrayLiteral("MATLAB 5.0 MAT-file")', block)
        self.assertIn('QLatin1String("preproc_")', block)

    def test_a_mat_file_that_is_not_a_project_is_refused_with_a_reason(self):
        self.assertIn("there is no set_sonic and no", SRC)

    def test_every_open_route_reaches_the_importer(self):
        #> The same fix the EddyPro import needed: the redirect sits outside
        #> the file-dialog branch, so the command line, Recent Files and the
        #> Finder reach it too.
        at = MW.index("EddyUhImport::looksLikeEddyUhProject(fileStr)")
        head = MW[:at]
        self.assertIn("importEddyProFile(fileStr);", head)
        self.assertIn("importEddyUhFile(fileStr);", MW[at:at + 400])


class TheEncodingsMatchEddyUHs(unittest.TestCase):

    @unittest.skipUnless(READ_RAW.is_file(), "EddyUH sources not present")
    def test_the_delimiter_codes(self):
        src = read(READ_RAW)
        #> EddyUH_ReadFileRaw.m:62 - 2 tab, 3 space, 4 comma, 5 semicolon.
        for code, name in ((2, "tab"), (3, "space"), (4, "comma"),
                           (5, "semicolon")):
            self.assertRegex(src, r"case %d\s*\n\s*%%.*\n\s*deli" % code)
            self.assertIn('QStringLiteral("%s")' % name, SRC, name)

    @unittest.skipUnless(COVAR.is_file(), "EddyUH sources not present")
    def test_the_detrend_codes_line_up_one_for_one(self):
        src = read(COVAR)
        self.assertIn("0 block-averaging", src)
        self.assertIn("1 linear detrending", src)
        self.assertIn("detrendType==2 %running mean", src)
        #> Which is why the importer passes the number straight through.
        self.assertIn("detrend >= 0 && detrend <= 2 ? detrend : 0", SRC)

    @unittest.skipUnless(COORDROT.is_file(), "EddyUH sources not present")
    def test_only_the_planar_fit_branch_is_keyed_on_four(self):
        src = read(COORDROT)
        self.assertIn("if D==4", src)
        self.assertIn("% planar fit", src)
        self.assertIn("if D >= 1", src)
        self.assertIn("if D >= 2", src)
        self.assertIn("if D == 3", src)

    def test_the_prototype_tokens_are_replaced_in_a_safe_order(self):
        #> MIN before MM, or the minute token is eaten by the month one -
        #> and MIN becomes MM, so it has to be parked first.
        block = SRC[SRC.index("QString convertPrototype"):]
        block = block[:block.index("\n}")]
        self.assertLess(block.index('QLatin1String("MIN")'),
                        block.index('QLatin1String("MM")'))
        self.assertLess(block.index('QLatin1String("DOY")'),
                        block.index('QLatin1String("DD")'))
        self.assertLess(block.index('QLatin1String("YYYY")'),
                        block.index('QLatin1String("YY")'))
        self.assertIn("parked", block)


class ItIsInTheBuildAndInTheMenu(unittest.TestCase):

    def test_both_files_are_listed(self):
        pri = read(SOURCES_PRI)
        self.assertIn("src/eddyuhimport.h", pri)
        self.assertIn("src/eddyuhimport.cpp", pri)

    def test_the_action_exists_and_is_connected_and_in_the_menu(self):
        self.assertIn("importEddyUhAction", read(MAINWINDOW_H))
        self.assertIn("Import EddyUH Project...", MW)
        self.assertIn("connect(importEddyUhAction, &QAction::triggered", MW)
        self.assertIn("fileMenu->addAction(importEddyUhAction);", MW)

    def test_the_file_dialog_filters_on_the_preproc_prefix(self):
        #> There is no extension to filter on.
        self.assertIn("EddyUH Preprocessing Setup (preproc_*)", MW)


if __name__ == "__main__":
    unittest.main()
