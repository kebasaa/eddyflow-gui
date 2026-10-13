"""Every help link reaches a page the documentation site has.

The question marks and help buttons pointed at keba_saa.github.io, a host
that does not exist (the site is kebasaa.github.io, see site_url in the
documentation's mkdocs.yml), and at page names from the old LI-COR help,
topics_EddyFlow/<Some_Page>.html. A few still pointed at licor.com, which
never hosted these pages. The MkDocs site serves each docs/topics/<name>.md
at topics/<name>/.

The offline-help branches still build paths under docs/help/topics_EddyFlow/
next to the executable; those are local files, not links, and are left out.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT.parent / "eddyflow-documentation" / "docs" / "topics"
SITE = "https://kebasaa.github.io/eddyflow-documentation/"
LINK = re.compile(re.escape(SITE) + r"topics/([a-z0-9\-]+)/(?:#([a-z0-9\-]+))?")
URL = re.compile(r"https?://[^\s\"')]+")


def sources():
    for path in sorted((ROOT / "src").glob("*")):
        if path.suffix in (".cpp", ".h"):
            yield path, path.read_text(encoding="utf-8", errors="replace")


def links():
    return [(path.name, m.group(1), m.group(2))
            for path, text in sources() for m in LINK.finditer(text)]


def slug(heading):
    """Python-Markdown's toc slugify, which MkDocs uses for heading ids."""
    text = re.sub(r"[^\w\s-]", "", heading).strip().lower()
    return re.sub(r"[-\s]+", "-", text)


class NoOldHostOrPageNames(unittest.TestCase):

    def test_no_source_uses_the_wrong_host(self):
        for path, text in sources():
            self.assertNotIn("keba_saa", text, path.name)

    def test_no_link_uses_the_old_page_names(self):
        for path, text in sources():
            for url in URL.findall(text):
                self.assertNotIn("topics_EddyFlow", url, path.name)

    def test_no_link_goes_to_licor_help(self):
        for path, text in sources():
            for url in URL.findall(text):
                self.assertNotIn("licor.com/env", url, path.name)

    def test_the_links_are_found(self):
        """Guards the pattern: a typo in it would let every check pass."""
        self.assertGreater(len(links()), 50)


@unittest.skipUnless(DOCS.is_dir(), "eddyflow-documentation is not checked out "
                     "next to eddyflow-gui")
class EachLinkHasItsPage(unittest.TestCase):

    def test_each_page_exists(self):
        for source, name, _ in links():
            self.assertTrue((DOCS / (name + ".md")).is_file(),
                            "%s links topics/%s/" % (source, name))

    def test_each_anchor_is_a_heading_of_its_page(self):
        for source, name, anchor in links():
            if not anchor:
                continue
            page = (DOCS / (name + ".md")).read_text(encoding="utf-8")
            ids = {slug(h) for h in re.findall(r"^#+\s+(.*?)\s*$", page, re.M)}
            self.assertIn(anchor, ids, "%s links topics/%s/#%s"
                          % (source, name, anchor))


if __name__ == "__main__":
    unittest.main()
