"""What a SmartFlux package refuses, rather than letting the module misread.

LI-COR's embedded EddyPro does not reject a value it lacks. It reads it as
one of its own, so a package carrying this fork's methods or gas layout runs
on the module and quietly computes something else. File > Export > EddyPro
replaces such settings and says so in its report; a SmartFlux package has no
report and runs unattended in the field, so smartfluxBlockReason() refuses it
instead, and says where to change the setting.

Three kinds of gap are pinned here:

  1. Two gases for one EddyPro slot. The writer keeps the first record that
     fits each of CO2, H2O, CH4 and the fourth slot, so a second CO2 analyser,
     or COS beside N2O, would be dropped from the package without a word,
     even with four gases or fewer in all.
  2. Methods EddyPro does not have: Vitale (2020) QC is read as Mauder and
     Foken, Billesbach and Lenschow random error as none, consecutive-
     difference despiking as Mauder (2013). The general export's replace
     rules are the list of these, and the guard is held to that list.
  3. The fourth gas's constants. EddyPro reads gas_mw <= 0 as N2O's 44.01
     (write_processing_project_variables.f90), and a record that leaves the
     species default in place stores -1. The package writes the species' own
     values instead, and refuses a species with none known.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
EC_PROJECT = GUI_ROOT / "src" / "ecproject.cpp"
EDDYPRO_EXPORT = GUI_ROOT / "src" / "eddyproexport.cpp"


def _read(path):
    return path.read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def _function(src, signature):
    body = src[src.index(signature):]
    return body[: body.index("\n}\n")]


def _branch(body, condition):
    """The text from \a condition to the end of the block it opens, with
    adjacent string literals joined so a message can be matched whole."""
    start = body.index(condition)
    rest = body[start:]
    return re.sub(r'"\s*\n\s*"', "", rest[: rest.index("\n    }\n")])


class TheGuard(unittest.TestCase):
    def setUp(self):
        self.src = _read(EC_PROJECT)
        self.body = _function(self.src, "QString EcProject::smartfluxBlockReason()")


class OneGasPerSlot(TheGuard):
    def test_gases_are_slotted_the_way_the_writer_slots_them(self):
        # The writer's rule is co2/h2o/ch4 pinned, everything else in the
        # fourth slot. legacySlotOfSpecies is the same rule; a guard counting
        # by any other would pass a project the writer then truncates.
        self.assertIn("inSlot[legacySlotOfSpecies(rec.slug)] << i;", self.body)
        helper = _function(self.src, "static int legacySlotOfSpecies(")
        for slug, slot in (("co2", 0), ("h2o", 1), ("ch4", 2)):
            self.assertIn('if (slug == QLatin1String("%s")) { return %d; }'
                          % (slug, slot), helper)
        self.assertIn("return 3;", helper)
        writer = _function(self.src, "void EcProject::writeEddyProCompatibleKeys(")
        self.assertIn('const char* pinned[3] = { "co2", "h2o", "ch4" };', writer)
        self.assertIn("int slot = 3;", writer)

    def test_only_records_with_a_column_count(self):
        loop = self.body[self.body.index("QVector<int> inSlot"):]
        loop = loop[: loop.index("inSlot[legacySlotOfSpecies")]
        self.assertIn("if (rec.rawColumn <= 0) { continue; }", loop)

    def test_a_second_gas_of_a_pinned_species_is_refused(self):
        branch = _branch(self.body, "if (inSlot[s].size() > 1)")
        self.assertIn("Deselect the others in Basic Settings.", branch)
        self.assertIn("for (int s = 0; s < kEddyProGasSlots - 1; ++s)", self.body)

    def test_two_gases_for_the_fourth_slot_are_refused(self):
        branch = _branch(self.body, "if (inSlot[3].size() > 1)")
        self.assertIn("Deselect the others in Basic Settings.", branch)

    def test_the_writer_no_longer_claims_more_than_the_guard_does(self):
        writer = _function(self.src, "void EcProject::writeEddyProCompatibleKeys(")
        self.assertIn("refused before it gets here", writer)


class MethodsEddyProLacks(TheGuard):
    #: The general export's replace rules, keyed by ini constant, mapped to
    #: the state field the guard reads.
    STATE_FIELD = {
        "INI_SCREEN_SETTINGS_7": "screenSetting.tlag_meth",
        "INI_PROJECT_46": "projectGeneral.hf_meth",
        "INI_PROJECT_55": "projectGeneral.qcflag_meth",
        "INI_RAND_ERROR_0": "randomError.ru_method",
        "INI_SCREEN_PARAM_60": "screenParam.despike_vm",
    }

    def _replace_rules(self):
        """{ini constant: unsupported values} from eddyproexport.cpp."""
        src = _read(EDDYPRO_EXPORT)
        table = src[src.index("const QVector<OptionRule>& optionRules()"):]
        table = table[: table.index("// ---- options EddyPro does not have")]
        rules = {}
        for rule in table.split("\n        { EcIni::")[1:]:
            match = re.match(r"\w+, EcIni::(\w+),.*\{ ([\d, ]+) \}, \d+, false\b",
                             rule, re.S)
            if match:
                rules[match.group(1)] = [int(v) for v in match.group(2).split(",")]
        return rules

    def test_the_rule_table_parses(self):
        self.assertEqual(set(self.STATE_FIELD), set(self._replace_rules()),
                         "a replace rule was added or renamed in the general "
                         "export; map it here and refuse it in the guard")

    def test_every_value_the_general_export_replaces_is_refused(self):
        # A value File > Export has to replace is one the module would
        # misread, so a package carrying it must not be built.
        for key, values in self._replace_rules().items():
            field = self.STATE_FIELD[key]
            short = field.split(".")[1]
            for value in values:
                self.assertRegex(
                    self.body,
                    r"%s\s*==\s*%d\b" % (re.escape(short), value),
                    "%s %d is replaced by the general export but packaged "
                    "for SmartFlux as it is" % (field, value))
                self.assertIn(field.split(".")[0], self.body)

    def test_each_refusal_says_where_to_change_it(self):
        for condition, place in (
                ("qcflag_meth == 4", "Advanced Settings > Processing Options."),
                ("ru_method == 4", "Statistical Analysis."),
                ("despike_vm == 2", "Statistical Analysis.")):
            branch = _branch(self.body, condition)
            self.assertIn(place, branch,
                          "the refusal for %s must name %s" % (condition, place))

    def test_despiking_is_refused_only_while_it_runs(self):
        branch = _branch(self.body, "if (ec_project_state_.screenTest.test_sr != 0")
        self.assertIn("despike_vm == 2", branch)


class TheFourthGasKeepsItsConstants(TheGuard):
    def setUp(self):
        super().setUp()
        self.writer = _function(self.src, "void EcProject::writeEddyProCompatibleKeys(")
        self.helper = _function(self.src, "static void eddyProFourthGasConstants(")

    def test_the_species_default_is_resolved(self):
        self.assertIn("GasMetadata::findSpecies(rec->slug)", self.helper)
        self.assertIn("entry->molecularWeight", self.helper)
        self.assertIn("entry->diffusivity", self.helper)

    def test_n2o_keeps_the_sentinel_the_reference_holds(self):
        self.assertIn('if (rec->slug == QLatin1String("n2o")) { return; }',
                      self.helper)

    def test_the_writer_uses_it(self):
        self.assertIn("eddyProFourthGasConstants(", self.writer)
        self.assertNotIn("gases.at(slotOf[3]).mw", self.writer,
                         "the record's -1 is N2O's constant to EddyPro")
        # Zero is as much "use N2O's" to EddyPro as -1 is.
        self.assertIn("openMw > 0.0 ? openMw : -1.0", self.writer)
        self.assertIn("openDiff > 0.0 ? openDiff : -1.0", self.writer)

    def test_a_species_with_no_constant_is_refused(self):
        branch = _branch(self.body, "if (!inSlot[3].isEmpty())")
        self.assertIn("eddyProFourthGasConstants(", branch)
        self.assertIn("No molecular weight is known", branch)
        self.assertIn("No diffusivity is known", branch)
        self.assertIn("Enter it in Basic Settings.", branch)

    def test_the_helper_is_defined_before_its_callers(self):
        at = self.src.index("static void eddyProFourthGasConstants(")
        self.assertLess(at, self.src.index("void EcProject::writeEddyProCompatibleKeys("))
        self.assertLess(at, self.src.index("QString EcProject::smartfluxBlockReason()"))


if __name__ == "__main__":
    unittest.main()
