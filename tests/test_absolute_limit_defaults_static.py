"""Every species must resolve to a window it can actually be measured in.

Absolute limits are stored on one basis - umol/mol for every gas but water,
which is on mmol/mol - and the numbers in `ecprojectstate.h` are already on it.
That is easy to get wrong by a factor of a thousand, and getting it wrong is
silent in the worst direction: with `filter_al=1` an absolute limit does not
flag an out-of-range sample, it DELETES it. A ceiling of 1 umol/mol on methane
would sit below the ~1.92 umol/mol background and throw away every reading.

So the checks here are arithmetic against measured abundances rather than
against the literals in the table. They fail if a ceiling drops below what the
atmosphere actually contains, whoever edits the registry and for whatever
reason.

Backgrounds, all in the stored basis:
  NOAA GML 2024 global means - CO2 422.8 umol/mol, CH4 1.92 umol/mol,
  N2O 0.338 umol/mol; CO 0.04-0.15 umol/mol; COS ~0.0005 umol/mol; and the
  major constituents N2 780900, O2 209500, Ar 9340 umol/mol.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent


def read(rel):
    return (GUI_ROOT / rel).read_text(encoding="utf-8", errors="replace")


#: Species -> its atmospheric abundance in the stored basis. Water is absent
#: deliberately: it is not a background gas, it spans four orders of magnitude,
#: and its ceiling is a saturation bound rather than a multiple of anything.
ABUNDANCE = {
    "co2": 422.8,
    "ch4": 1.92,
    "n2o": 0.338,
    "co": 0.15,
    "cos": 0.0005,
    "n2": 780900.0,
    "o2": 209500.0,
    "ar": 9340.0,
}

GENERIC = 5.0


def registry_ceilings():
    """slug -> explicit ceiling, for the rows that state one."""
    body = read("src/gas_metadata.cpp")
    out = {}
    #> The rows are brace-initialised, and the ceiling is the sixth field when
    #> present: formula, weight, diffusivity, status, floor, ceiling.
    row = re.compile(
        r"\{\s*(?P<formula>[^,]+),\s*[\d.]+,\s*[\d.]+,\s*"
        r"DiffusivityStatus::\w+\s*,\s*(?P<floor>[\d.e+-]+)\s*,\s*"
        r"(?P<ceiling>[\d.e+-]+)\s*\}")
    digit = {"TWO": "2", "THREE": "3", "FOUR": "4"}
    for m in row.finditer(body):
        #> The formulas are assembled from quoted pieces and Defs::SUB* tokens,
        #> in source order - so the slug is those pieces concatenated, with each
        #> subscript following the letter it belongs to.
        rebuilt = ""
        for a, b, sub in re.findall(
                r"'([A-Za-z0-9])'|\"([A-Za-z0-9]+)\"|SUB(TWO|THREE|FOUR)",
                m.group("formula")):
            rebuilt += digit[sub] if sub else (a or b)
        out[rebuilt.lower()] = float(m.group("ceiling"))
    return out


class AbsoluteLimitDefaultStaticTests(unittest.TestCase):
    def test_the_generic_ceiling_is_5000_of_the_trace_gas_unit(self):
        """5000 nmol/mol, which is 5 umol/mol stored."""
        head = read("src/gas_metadata.h")
        assert f"genericAbsoluteLimitMax = {GENERIC}" in head, head[:0] or (
            "the generic ceiling is not 5 umol/mol"
        )

    def test_no_species_ceiling_sits_below_its_own_abundance(self):
        """The assertion that would have caught a factor-of-1000 slip. A ceiling
        under the background deletes every real sample."""
        ceilings = registry_ceilings()
        #> Named explicitly, because a species the parser missed would fall back
        #> to the generic 5 and quietly pass this for anything below 5 - which
        #> is methane, CO and COS, three of the six that matter.
        for slug in ("co2", "ch4", "n2o", "co", "cos", "h2o"):
            assert slug in ceilings, (
                f"{slug} has no explicit ceiling in the registry, or the row "
                f"format changed and the parser no longer sees it"
            )
        too_low = []
        for slug, abundance in ABUNDANCE.items():
            ceiling = ceilings.get(slug, GENERIC)
            if ceiling <= abundance:
                too_low.append(f"{slug}: ceiling {ceiling} <= abundance {abundance}")
        assert not too_low, (
            "ceilings below what the atmosphere contains:\n  " + "\n  ".join(too_low)
        )

    def test_the_major_constituents_state_their_own(self):
        """N2, O2 and Ar sit far above the generic, so they cannot rely on it."""
        ceilings = registry_ceilings()
        for slug in ("n2", "o2", "ar"):
            assert slug in ceilings, (
                f"{slug} has no explicit ceiling, so it falls to the generic "
                f"{GENERIC} umol/mol - three to five orders below its abundance"
            )

    def test_the_ceiling_is_never_zero_at_the_point_of_use(self):
        """max <= min is how the engine says "limits absent". The accessor must
        substitute the generic rather than pass a zero through."""
        body = read("src/gas_metadata.cpp")
        fn = body[body.index("double defaultAbsoluteLimitMax"):]
        fn = fn[:fn.index("\n}")]
        assert "absoluteLimitMax > 0.0" in fn
        assert "return genericAbsoluteLimitMax;" in fn

    def test_the_seeding_uses_the_species_ceiling(self):
        body = read("src/ecproject.cpp")
        assert "proc.alMax = GasMetadata::defaultAbsoluteLimitMax(slug);" in body, (
            "new gas records are still seeded from the shared 'other' pair"
        )

    def test_water_keeps_its_own_basis(self):
        """Water is the one row on the mmol/mol basis. 40 is its ceiling there;
        read as umol/mol it would be a hundredth of ambient."""
        assert registry_ceilings().get("h2o") == 40.0


if __name__ == "__main__":
    unittest.main()
