"""A two-digit hex escape must not be followed by another hex digit.

C++ `\\x` escapes are greedy: they consume as many hex digits as follow, not
two. So the UTF-8 minus sign written as `\\xe2\\x88\\x92` is fine on its own and
broken the moment a digit comes next - `\\x921` is one escape worth 0x921, which
does not fit in a char. The compiler warns "hex escape sequence out of range"
and the text the user reads is mangled.

This bit the singularity-band tooltip, where every "-1", "-1.2" and "-0.8" was
written with a real minus sign and every one of them was corrupted. The fix is
to close the literal and reopen it - `"\\xe2\\x88\\x92" "1"` - because adjacent
string literals concatenate and the escape ends at the quote.

The tooltips in this program carry the explanation of the science, so a mangled
one is not cosmetic. Cheap to check, and the compiler warning is easy to miss in
a full build.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
BACKSLASH = chr(92)

#> Built from chr(92) rather than written as a literal: this file is itself
#> edited by tooling that has been known to eat doubled backslashes.
GREEDY = re.compile(re.escape(BACKSLASH) + r"x[0-9a-fA-F]{2}(?=[0-9a-fA-F])")


class NoHexEscapeSwallowsWhatFollowsIt(unittest.TestCase):
    def test_no_source_file_has_one(self):
        offenders = []
        for path in sorted((ROOT / "src").rglob("*")):
            if path.suffix not in (".cpp", ".h"):
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for number, line in enumerate(text.splitlines(), 1):
                for hit in GREEDY.findall(line):
                    offenders.append("%s:%d %s" % (path.name, number, hit))
        self.assertEqual(
            offenders, [],
            "a hex escape runs into the character after it, so that character "
            "is swallowed into the escape and the string is corrupted. Close "
            "the literal and reopen it: "
            + BACKSLASH + 'xe2' + BACKSLASH + 'x88' + BACKSLASH + 'x92" "1'
            + ".\n  " + "\n  ".join(offenders))

    def test_the_check_can_see_one(self):
        """Guard against the pattern quietly matching nothing forever."""
        sample = 'tr("approaches ' + BACKSLASH + 'xe2' + BACKSLASH + 'x88' \
                 + BACKSLASH + 'x921 and divides")'
        self.assertEqual(len(GREEDY.findall(sample)), 1)
        fixed = 'tr("approaches ' + BACKSLASH + 'xe2' + BACKSLASH + 'x88' \
                + BACKSLASH + 'x92" "1 and divides")'
        self.assertEqual(GREEDY.findall(fixed), [])


if __name__ == "__main__":
    unittest.main()
