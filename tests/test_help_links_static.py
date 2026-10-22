"""Every help link reaches a page, online and in the offline bundle.

The question marks and help buttons pointed at keba_saa.github.io, a host
that does not exist (the site is kebasaa.github.io, see site_url in the
documentation's mkdocs.yml), and at page names from the old LI-COR help,
topics_EddyFlow/<Some_Page>.html. A few still pointed at licor.com, which
never hosted these pages. The MkDocs site serves each docs/topics/<name>.md
at topics/<name>/.

The Help menu then opened the documentation's git repository rather than the
site it publishes, and the "Instruction Manual (PDF)" entry opened a LI-COR
Box share that no longer resolves.

Offline, docs/help holds a build of that same site made with
--no-directory-urls, so topics/<name>/ is the file topics/<name>.html, which
is what WidgetUtils::localHelpUrl() looks for. A page the bundle does not
carry falls back to the online URL, so a stale bundle degrades rather than
breaking - which is exactly why the bundle needs a test of its own.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT.parent / "eddyflow-documentation" / "docs" / "topics"
HELP = ROOT / "docs" / "help"
SITE = "https://kebasaa.github.io/eddyflow-documentation/"
LINK = re.compile(re.escape(SITE) + r"topics/([a-z0-9\-]+)/(?:#([a-z0-9\-]+))?")
URL = re.compile(r"https?://[^\s\"')]+")

# the dialogs that strip the native title bar help button, and the page each
# one documents; every entry reaches its page through createQuestionMarkButton
DIALOG_PAGES = {
    "cecsettingsdialog.cpp": "topics/cec-settings/",
    "langfordsettingsdialog.cpp": "topics/limit-of-detection-settings/",
    "detlimsettingsdialog.cpp": "topics/flux-detection-limit-settings/",
    "planarfitsettingsdialog.cpp": "topics/planar-fit/",
    "pwbtimelagsettingsdialog.cpp": "topics/pwb-time-lag-settings/",
    "timelagpreassessdialog.cpp": "topics/timelag-preliminary-assessment/",
    "timelagsettingsdialog.cpp": "topics/time-lag-detect-correct/",
    "binarysettingsdialog.cpp": "topics/generic-binary-files/",
    "rawfilenamedialog.cpp": "topics/raw-file-name-format/",
    "remotebrowsedialog.cpp": "topics/remote-folders/",
    "rawfilesettingsdialog.cpp": "topics/metadata-file-editor/#raw-file-settings",
    "dlrawfiledesctab.cpp": "topics/metadata-file-editor/#raw-file-description",
}


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

    def test_no_link_goes_to_the_dead_box_shares(self):
        """The PDF manual and quick start were LI-COR Box links."""
        for path, text in sources():
            self.assertNotIn("boxenterprise.net", text, path.name)

    def test_no_help_link_opens_the_documentation_repository(self):
        """The repository is the source; a reader wants the published site."""
        for path, text in sources():
            for url in URL.findall(text):
                self.assertNotIn("github.com/kebasaa/eddyflow-documentation",
                                 url, path.name)

    def test_nothing_reads_the_retired_flare_bundle(self):
        """docs/support held the LI-COR MadCap help and is gone."""
        for path, text in sources():
            self.assertNotIn("docs/support", text, path.name)

    def test_the_links_are_found(self):
        """Guards the pattern: a typo in it would let every check pass."""
        self.assertGreater(len(links()), 50)


class TheHelpMenuReachesTheSite(unittest.TestCase):

    def test_the_help_entry_opens_the_documentation_root(self):
        page = (ROOT / "src" / "mainwindow.cpp").read_text(
            encoding="utf-8", errors="replace")
        start = page.index("void MainWindow::showHelp()")
        self.assertIn("Defs::EP_DOCUMENTATION",
                      page[start:page.index("}", start)])

    def test_defs_states_the_site_once(self):
        defs = (ROOT / "src" / "defs.h").read_text(
            encoding="utf-8", errors="replace")
        self.assertIn('EP_DOCUMENTATION = QStringLiteral("%s")' % SITE, defs)


class HelpOpensInAReleaseBuild(unittest.TestCase):
    """The page has to be opened outside the qDebug() statement.

    A release build defines QT_NO_DEBUG_OUTPUT (see the else branch of
    EddyFlow_win.pro), and Qt then expands qDebug to
    "while (false) QMessageLogger().noDebug", so nothing in the statement
    runs - a call nested in its arguments included. Written as
    qDebug() << url << QDesktopServices::openUrl(url), every question mark
    worked in a debug build and did nothing at all in a release one.
    """

    MACRO = re.compile(r"\b(?:qDebug|qWarning|qInfo|qErrnoWarning)\s*\(\s*\)")

    def statements(self):
        for path, text in sources():
            for match in self.MACRO.finditer(text):
                end = text.find(";", match.end())
                if end < 0:
                    continue
                yield path.name, text[:match.start()].count("\n") + 1, \
                    text[match.end():end]

    def test_no_debug_statement_opens_a_url(self):
        for name, line, statement in self.statements():
            self.assertNotIn("openUrl", statement,
                             "%s:%d opens a URL inside a debug statement, "
                             "which a release build compiles away" % (name, line))


class TheSpectroscopicBoxesHaveHelp(unittest.TestCase):
    """Both are described under the WPL section of the processing options."""

    def test_each_box_has_a_question_mark_to_its_section(self):
        page = (ROOT / "src" / "advprocessingoptions.cpp").read_text(
            encoding="utf-8", errors="replace")
        for box in ("spectroCheckBox", "spectroWaterCheckBox"):
            start = page.index(box + "->setQuestionMark(")
            self.assertIn(SITE + "topics/raw-processing-options/"
                          "#compensation-for-density-fluctuations-wpl-terms",
                          page[start:page.index("\n", start)], box)


class EveryDialogOffersItsPage(unittest.TestCase):

    def test_each_dialog_has_a_question_mark_to_its_page(self):
        for name, target in DIALOG_PAGES.items():
            page = (ROOT / "src" / name).read_text(
                encoding="utf-8", errors="replace")
            self.assertIn("WidgetUtils::createQuestionMarkButton", page,
                          "%s offers no help" % name)
            self.assertIn(SITE + target, page, name)


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


@unittest.skipUnless(HELP.is_dir(), "docs/help is not generated; run "
                     "scripts/update-offline-help.ps1")
class TheOfflineBundleCarriesEveryPage(unittest.TestCase):

    def test_each_linked_page_has_a_local_file(self):
        for source, name, _ in links():
            self.assertTrue((HELP / "topics" / (name + ".html")).is_file(),
                            "%s links topics/%s/, which docs/help lacks - "
                            "regenerate it" % (source, name))

    def test_the_bundle_was_built_without_directory_urls(self):
        """topics/<name>/index.html would make every link dead over file://."""
        self.assertTrue((HELP / "index.html").is_file())
        self.assertFalse((HELP / "topics" / "planar-fit" / "index.html").is_file(),
                         "docs/help was built with directory URLs")

    def test_the_mapping_looks_under_docs_help(self):
        page = (ROOT / "src" / "widget_utils.cpp").read_text(
            encoding="utf-8", errors="replace")
        start = page.index("QUrl WidgetUtils::localHelpUrl")
        body = page[start:page.index("\n}", start)]
        self.assertIn("Defs::DOC_DIR", body)
        self.assertIn('QStringLiteral("/help/")', body)


if __name__ == "__main__":
    unittest.main()
