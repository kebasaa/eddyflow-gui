"""The extended .ghg stand-in must be understood on both sides.

An archive describing an instrument EddyPro cannot name declares it twice: a
generic stand-in in `instr_<k>_model`, which EddyPro validates, and the truth in
`instr_<k>_ef_model`, which EddyPro ignores and both EddyFlow programs prefer.

Two ways that quietly breaks, neither of which the compiler or a runtime test
would catch:

1. **The key spellings drift.** Nothing at build time connects this repository
   to the engine's tag table, so `ef_model` here and `ef_model` there are two
   independent string literals that happen to match today.

2. **The manufacturer is believed.** The one written beside a stand-in describes
   the STAND-IN - an EC150 declared as generic_open_path says `other_irga` - so
   reading the real model without re-deriving the manufacturer leaves the row
   claiming Other/EC150. That is not merely untidy: the model combobox is
   filtered by the manufacturer beside it, so the real model is not even
   offered, and the pair is what gets written to a clean metadata on save.

Also guarded: that saving still WIPES the extension keys. That is deliberate,
not an oversight - a standalone .metadata is under no obligation to EddyPro and
names the real instrument outright - and it is the kind of deliberate omission
someone later "fixes".

Engine-side assertions are skipped, not failed, when the engine checkout is not
beside this one.
"""

import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_GLOBALS = ENGINE_ROOT / "src" / "src_common" / "m_common_global_var.f90"

DL_INIDEFS = GUI_ROOT / "src" / "dlinidefs.h"
DL_PROJECT = GUI_ROOT / "src" / "dlproject.cpp"
IRGA_DESC = GUI_ROOT / "src" / "irga_desc.cpp"
ANEM_DESC = GUI_ROOT / "src" / "anem_desc.cpp"

#: The one key the format adds per instrument block. Spelled once here so a
#: rename has to come through this file.
EF_MODEL = "ef_model"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


class KeySpelling(unittest.TestCase):

    def test_the_gui_declares_it_once_for_both_categories(self):
        src = read(DL_INIDEFS)
        self.assertIn(f'INI_INSTR_EF_MODEL   = QStringLiteral("{EF_MODEL}")', src)
        #> Declared once on purpose: it is one key, applied by the engine in a
        #> single loop over every instr_<K>_* block whatever the category. A
        #> second constant with the same spelling is how the two categories
        #> drift apart.
        self.assertEqual(1, src.count(f'QStringLiteral("{EF_MODEL}")'))

    def test_the_engine_names_the_same_key(self):
        if not ENGINE_GLOBALS.is_file():
            self.skipTest("engine checkout not beside this one")
        src = read(ENGINE_GLOBALS)
        #> The engine's tag tables are generated and positional, so the label
        #> appears once per instrument slot. Any occurrence proves the spelling.
        self.assertIn(f"'instr_1_{EF_MODEL}'", src)


class LoadPrefersTheRealInstrument(unittest.TestCase):

    def setUp(self):
        self.src = read(DL_PROJECT)

    def _resolver(self):
        body = self.src[self.src.index("DlProject::ModelKeys DlProject::instrumentModelKeys"):]
        return body[:body.index("\nDlProject::InstrumentType")]

    def test_both_branches_go_through_the_shared_resolver(self):
        #> Anemometers and analysers both, and through one helper rather than
        #> two copies of the precedence - the engine has exactly one.
        self.assertEqual(
            2, self.src.count("instrumentModelKeys(project_ini, prefix,"))

    def test_the_resolver_prefers_ef_model_and_falls_back(self):
        body = self._resolver()
        self.assertIn("DlIni::INI_INSTR_EF_MODEL", body)
        #> Falls back to whatever key the caller names, rather than to a
        #> hard-coded `model`, so the sonic and analyser spellings stay the
        #> callers' business.
        self.assertIn("iniGroup.value(prefix + modelKey)", body)

    def test_the_index_suffix_is_stripped_from_both(self):
        #> Every model value carries a trailing `_<n>` pairing it with an
        #> instrument. An ef_model that kept its suffix would match no model
        #> string at all, and the row would come back empty.
        self.assertEqual(2, self._resolver().count("remove(indexSuffix)"))

    def test_an_unknown_ef_model_degrades_to_the_stand_in(self):
        #> The whole point of the format is a reader that does not recognise
        #> the instrument, and one day this build is that reader. An ef_model
        #> newer than this release resolves to no model at all, so taking it
        #> would leave the row EMPTIER than the stand-in it replaced - a
        #> regression on exactly the file the format exists to carry.
        #>
        #> Both keys are kept for this, which is why the resolver returns a
        #> pair rather than one string.
        for kind in ("Anem", "Irga"):
            self.assertIn(
                f"fromIni{kind}Model({kind.lower()}Keys.effective).isEmpty()",
                self.src)
        self.assertIn("anemKeys.effective = anemKeys.standIn", self.src)
        self.assertIn("irgaKeys.effective = irgaKeys.standIn", self.src)
        #> and stops calling it an override, so the manufacturer beside the
        #> stand-in - which is the RIGHT one for a stand-in - is left alone.
        self.assertIn("anemKeys.overridden = false;", self.src)
        self.assertIn("irgaKeys.overridden = false;", self.src)

    def test_the_manufacturer_is_rederived_when_it_stood_in(self):
        for kind in ("Anem", "Irga"):
            self.assertIn(f"{kind}Desc::manufacturerForModel(", self.src)
        #> Gated on the override having happened. Re-deriving unconditionally
        #> would rewrite the manufacturer of every classic file that is merely
        #> opened, which this has no business touching.
        self.assertIn("if (anemStoodIn)", self.src)
        self.assertIn("if (irgaStoodIn)", self.src)


class ManufacturerDerivation(unittest.TestCase):

    def test_it_asks_the_existing_lists(self):
        #> Built from the per-manufacturer lists the combobox already uses, not
        #> from a second table: a model added to one of them must be answered
        #> for here without being added twice.
        irga = read(IRGA_DESC)
        body = irga[irga.index("const QString IrgaDesc::manufacturerForModel"):]
        body = body[:body.index("\nbool IrgaDesc::isWellNamed")]
        for lst in ("licorModelStringList", "campbellIrgaModelStringList",
                    "miroModelStringList", "aerodyneModelStringList",
                    "otherModelStringList"):
            self.assertIn(f"{lst}().contains(model)", body)

        anem = read(ANEM_DESC)
        body = anem[anem.index("const QString AnemDesc::manufacturerForModel"):]
        body = body[:body.index("\n// Return string list of anem types")]
        for lst in ("campbellModelStringList", "gillModelStringList",
                    "metekModelStringList", "youngModelStringList",
                    "otherModelStringList"):
            self.assertIn(f"{lst}().contains(model)", body)

    def test_an_unclaimed_model_returns_empty_rather_than_a_guess(self):
        #> The caller leaves the file's manufacturer alone on empty, so an
        #> unrecognised model degrades to what the file said instead of to
        #> nothing.
        for path, sig, end in (
                (IRGA_DESC, "const QString IrgaDesc::manufacturerForModel",
                 "\nbool IrgaDesc::isWellNamed"),
                (ANEM_DESC, "const QString AnemDesc::manufacturerForModel",
                 "\n// Return string list of anem types")):
            src = read(path)
            body = src[src.index(sig):]
            body = body[:body.index(end)]
            self.assertIn("return QString();", body)


class SaveDropsTheExtensionOnPurpose(unittest.TestCase):

    def test_the_two_groups_are_still_cleared(self):
        src = read(DL_PROJECT)
        #> Both wipes stay. A standalone .metadata names the real instrument in
        #> `model`, so the stand-in and its ef_model have nothing left to
        #> describe, and ghg_format_version describes a format this file is not
        #> written in. Preserving them would make the file say the same thing
        #> twice, differently.
        self.assertEqual(2, src.count("project_ini.remove(QString());"))

    def test_the_gui_never_writes_the_key(self):
        src = read(DL_PROJECT)
        self.assertNotIn("setValue(prefix + DlIni::INI_INSTR_EF_MODEL", src)


if __name__ == "__main__":
    unittest.main()
