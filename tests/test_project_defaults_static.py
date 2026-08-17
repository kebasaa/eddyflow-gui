"""Static check on the fallbacks in EcProject's project-file loader.

Every setting is read as

    ec_project_state_.<group>.<field>
            = project_ini.value(EcIni::<KEY>,
                                defaultEcProjectState.<group>.<field>)...

three names that have to agree, spelled out 364 times. Naming the wrong field
in the third position is invisible: the key is still right, so a project that
states the setting loads correctly and only one that omits it is wrong - and
which settings a project omits changes with the file format.

That is what happened to CO2's spectral frequency range. The fallback for
`sa_fmin_co2` named `sa_fmax_co2`, so once the record format stopped writing
the flat `sa_fmin_co2` key, every round-tripped project took CO2's upper bound
as its lower one and carried fmin == fmax onto its CO2 gas records. The typo
had been there since the first commit and cost nothing until the writer
changed underneath it.

This check reads the pairing off the source. It cannot know which field is
*meant*, but it can insist that the two sides match, which is the invariant the
364 correct lines already keep.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
EC_PROJECT = GUI_ROOT / "src" / "ecproject.cpp"

#: `ec_project_state_.<g>.<f> = project_ini.value(EcIni::<KEY>,
#:  defaultEcProjectState.<g2>.<f2>)`, across the line breaks the file uses.
ASSIGNMENT = re.compile(
    r"ec_project_state_\.(\w+)\.(\w+)\s*=\s*project_ini\.value\("
    r"\s*EcIni::(\w+)\s*,\s*defaultEcProjectState\.(\w+)\.(\w+)\s*\)",
    re.S)

#: Fallbacks that deliberately name another field, with the reason.
#:
#: The magnetic declination date has no default of its own - its own field is
#: an empty QString - and takes the project's end date instead. `applyDefaults`
#: (ecproject.cpp:938) does the same assignment, which is what makes it
#: deliberate rather than a second instance of the bug above.
DELIBERATE = {
    ("screenGeneral", "dec_date"): ("projectGeneral", "end_date"),
}

#: Below this the regex has stopped matching the file and the check is empty.
MIN_ASSIGNMENTS = 300


class LoaderFallbacks(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        src = EC_PROJECT.read_text(encoding="utf-8", errors="replace")
        cls.matches = []
        for m in ASSIGNMENT.finditer(src):
            line = src[:m.start()].count("\n") + 1
            cls.matches.append((line,) + m.groups())

    def test_the_pattern_still_matches_the_file(self):
        """A refactor of the loader's shape would silently empty this check."""
        self.assertGreaterEqual(
            len(self.matches), MIN_ASSIGNMENTS,
            "only %d assignments matched - the regex no longer describes the "
            "loader, so this check is asserting nothing" % len(self.matches))

    def test_every_fallback_names_the_field_it_fills(self):
        failures = []
        for line, group, field, key, dgroup, dfield in self.matches:
            if (group, field) == (dgroup, dfield):
                continue
            if DELIBERATE.get((group, field)) == (dgroup, dfield):
                continue
            failures.append(
                "ecproject.cpp:%d  %s.%s is filled from "
                "defaultEcProjectState.%s.%s (key %s)"
                % (line, group, field, dgroup, dfield, key))

        self.assertEqual(
            [], failures,
            "a fallback naming another field is only wrong for a project that "
            "omits the key, so it survives every test that loads a complete "
            "file:\n  " + "\n  ".join(failures))

    def test_co2_spectral_fmin_falls_back_to_itself(self):
        """The instance that prompted this check, pinned by name."""
        pair = [m for m in self.matches if m[2] == "sa_fmin_co2"]
        self.assertEqual(1, len(pair), "expected one sa_fmin_co2 assignment")
        _, group, field, _, dgroup, dfield = pair[0]
        self.assertEqual((group, field), (dgroup, dfield),
                         "sa_fmin_co2 must not fall back to sa_fmax_co2 - that "
                         "gives CO2 an empty spectral frequency range")


class DegenerateRangeRepair(unittest.TestCase):
    """Projects saved before the fix carry fmin == fmax as a stated value, so
    the gap-filling repair has to recognise it as damage rather than intent."""

    def test_seeding_treats_an_inverted_range_as_unset(self):
        src = EC_PROJECT.read_text(encoding="utf-8", errors="replace")
        start = src.index("static void seedGasProcessingGaps")
        body = src[start: src.index("\n}\n", start)]
        self.assertIn("proc.saFmin >= proc.saFmax", body,
                      "an already-saved record holding fmin >= fmax is never "
                      "repaired by put(), which only fills sentinels")
        self.assertLess(body.index("proc.saFmin >= proc.saFmax"),
                        body.index("put(proc.saFmin"),
                        "the reset has to precede the put that would otherwise "
                        "keep the damaged value")


if __name__ == "__main__":
    unittest.main()
