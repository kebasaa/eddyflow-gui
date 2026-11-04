"""A newer release is offered, and on Windows and macOS installed in place, only on request.

The startup check reads the latest release from the GitHub API, which lists
the files a release carries; the release page it used to follow could only
give the version. Nothing is downloaded until the user clicks "Download and
install", the download can be cancelled, and nothing is installed until the
user agrees a second time, once the download is ready - and then only after
the main window may close (a computation stopped, a project saved), the same
question closing asks.

Installing renames each file being replaced to <name>.old, as Windows will
rename a running program but not overwrite it, and puts everything back if a
step fails. The next start removes the .old files the update listed in
.update/replaced.txt, and nothing else.

On macOS the whole EddyFlow.app is replaced, from the release's .dmg: it is
mounted with hdiutil, its app copied with ditto, the copy's signature
checked, and the running bundle renamed into .eddyflow-update/old beside it
before the new one takes its place - and renamed back if that fails. Nothing
is written inside the bundle. The new version starts through open -n, and the
next start removes the old bundle. On Linux there is only a button to the
download page, which is the latest release page.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"


def read(name):
    return (SRC / name).read_text(encoding="utf-8")


def body(text, signature):
    """The body of the function whose definition starts with signature."""
    start = text.index(signature)
    open_brace = text.index("{", start)
    depth = 0
    for i in range(open_brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[open_brace:i + 1]
    raise AssertionError("unbalanced braces after " + signature)


class TheCheck(unittest.TestCase):
    def test_asks_the_api_for_the_latest_release(self):
        defs = read("defs.h")
        self.assertIn('EP_LATEST_RELEASE_API = QStringLiteral('
                      '"https://api.github.com/repos/kebasaa/eddyflow-gui/releases/latest")', defs)
        execute = body(read("downloadmanager.cpp"), "void DownloadManager::execute()")
        self.assertIn("Defs::EP_LATEST_RELEASE_API", execute)

    def test_the_download_page_is_the_latest_release(self):
        defs = read("defs.h")
        self.assertIn('EP_LATEST_RELEASE = QStringLiteral('
                      '"https://github.com/kebasaa/eddyflow-gui/releases/latest")', defs)
        self.assertNotIn("releases/tag/latest", defs)
        page = body(read("updatedialog.cpp"), "void UpdateDialog::showDownloadPage()")
        self.assertIn("QDesktopServices::openUrl(QUrl(Defs::EP_LATEST_RELEASE))", page)

    def test_takes_a_zip_or_a_7z(self):
        parse = body(read("downloadmanager.cpp"), "void DownloadManager::parseRelease(")
        self.assertIn('".zip"', parse)
        self.assertIn('".7z"', parse)
        self.assertIn('"digest"', parse)
        extract = body(read("updateinstaller.cpp"), "void UpdateInstaller::extract(")
        self.assertIn("JlCompress::extractDir", extract)
        self.assertIn("sevenZip()", extract)

    def test_takes_the_archive_for_this_build_architecture(self):
        manager = read("downloadmanager.cpp")
        self.assertIn('"_windows_([A-Za-z0-9]+)\\\\.(zip|7z)$"', manager)
        self.assertIn("QSysInfo::buildCpuArchitecture()", body(manager, "QString releaseArch()"))
        self.assertNotIn("currentCpuArchitecture", manager)
        parse = body(manager, "void DownloadManager::parseRelease(")
        self.assertIn("releaseArch()", parse)
        # an archive with no architecture in its name predates arm64
        self.assertIn(': QStringLiteral("x86_64");', parse)
        # on macOS, the disk image for this architecture
        self.assertIn('"_macos_([A-Za-z0-9]+)\\\\.dmg$"', parse)

    def test_startup_shows_only_a_newer_release_and_waits_for_no_timer(self):
        main = read("mainwindow.cpp")
        self.assertNotIn("showAutoUpdateResults", main)
        self.assertNotIn("showAutoUpdateResults", read("mainwindow.h"))
        create = body(main, "void MainWindow::createUpdateDialog()")
        self.assertIn("&UpdateDialog::checkFinished", create)
        self.assertIn("if (newer && !updateDialog->isVisible())", create)


class AskedTwice(unittest.TestCase):
    def test_downloads_only_from_the_download_button(self):
        dialog = read("updatedialog.cpp")
        self.assertEqual(len(re.findall(r"downloadFile\(", dialog)), 1)
        self.assertIn("updateManager->downloadFile(", body(dialog, "void UpdateDialog::startDownload()"))
        self.assertEqual(len(re.findall(r"&UpdateDialog::startDownload\b", dialog)), 1)
        self.assertIn("connect(downloadButton_, &QPushButton::clicked,\n"
                      "            this, &UpdateDialog::startDownload);", dialog)

    def test_installs_only_from_the_install_button(self):
        dialog = read("updatedialog.cpp")
        self.assertIn("connect(installButton_, &QPushButton::clicked,\n"
                      "            this, &UpdateDialog::installRequested);", dialog)
        self.assertNotIn("emit installRequested", dialog)

    def test_the_download_can_be_cancelled(self):
        dialog = read("updatedialog.cpp")
        start = body(dialog, "void UpdateDialog::startDownload()")
        self.assertIn("showButtons({ cancelButton_ });", start)
        self.assertIn("connect(cancelButton_, &QPushButton::clicked,\n"
                      "            this, &UpdateDialog::close);", dialog)
        self.assertIn("cancelUpdate();", body(dialog, "void UpdateDialog::close()"))
        cancel = body(dialog, "void UpdateDialog::cancelUpdate()")
        self.assertIn("updateManager->abortFile();", cancel)
        self.assertIn("installer_->discard();", cancel)
        # Esc and the window's close button cancel too
        self.assertIn("close();", body(dialog, "void UpdateDialog::reject()"))

    def test_installs_only_once_the_window_may_close(self):
        install = body(read("mainwindow.cpp"), "void MainWindow::installUpdate()")
        self.assertLess(install.index("continueBeforeClose()"),
                        install.index("installAndRestart("))


class InPlace(unittest.TestCase):
    def test_only_on_windows_and_macos(self):
        installer = read("updateinstaller.cpp")
        for signature in ("bool UpdateInstaller::isAvailable()",
                          "bool UpdateInstaller::install(QString* error)",
                          "void UpdateInstaller::cleanUp()"):
            text = body(installer, signature)
            self.assertIn("#if defined(Q_OS_WIN)", text, signature)
            self.assertIn("#elif defined(Q_OS_MACOS)", text, signature)
        available = body(installer, "bool UpdateInstaller::isAvailable()")
        self.assertIn("#else\n    return false;", available)

    def test_sets_aside_and_puts_back(self):
        install = body(read("updateinstaller.cpp"), "bool UpdateInstaller::install(QString* error)")
        self.assertIn("const QString aside = target + OLD_SUFFIX;", install)
        self.assertIn("QFile::rename(target, aside)", install)
        self.assertIn("QFile::rename(source, target)", install)
        # the undo, in the failure path
        self.assertIn("QFile::rename(target + OLD_SUFFIX, target);", install)

    def test_cleanup_removes_only_what_the_update_listed(self):
        installer = read("updateinstaller.cpp")
        clean = body(installer, "void UpdateInstaller::cleanUp()")
        self.assertIn("readReplacedList()", clean)
        self.assertIn("relative.endsWith(OLD_SUFFIX)", clean)
        self.assertIn('contains(QStringLiteral(".."))', clean)
        self.assertNotIn("*.old", installer)
        self.assertIn("UpdateInstaller::cleanUp();", read("main.cpp"))

    def test_macos_stages_beside_the_bundle(self):
        installer = read("updateinstaller.cpp")
        self.assertIn('UPDATE_STAGING_DIR_MAC = QStringLiteral(".eddyflow-update")', read("defs.h"))
        staging = body(installer, "QString UpdateInstaller::stagingDir()")
        self.assertIn("QFileInfo(bundle).absolutePath()", staging)
        self.assertIn("Defs::UPDATE_STAGING_DIR_MAC", staging)

    def test_macos_copies_the_app_out_of_the_dmg_and_checks_it(self):
        installer = read("updateinstaller.cpp")
        self.assertIn('"/usr/bin/hdiutil"', installer)
        copy = body(installer, "QString copyAppFromDiskImage(")
        self.assertIn('QStringLiteral("attach")', copy)
        self.assertIn("runTool(DITTO,", copy)
        self.assertIn('QStringLiteral("detach")', copy)
        # detached before the copy is judged, whatever happened
        self.assertLess(copy.index('QStringLiteral("detach")'), copy.index("runTool(CODESIGN,"))
        self.assertIn('QStringLiteral("--strict")', copy)
        self.assertIn("bundleIdentifier(copy) != id", copy)
        self.assertIn('".dmg"', body(installer, "bool UpdateInstaller::canExtract("))

    def test_macos_swaps_the_bundle_and_puts_it_back(self):
        install = body(read("updateinstaller.cpp"), "bool UpdateInstaller::install(QString* error)")
        mac = install[install.index("#elif defined(Q_OS_MACOS)"):]
        self.assertIn("dir.rename(bundlePath_, aside)", mac)
        self.assertIn("dir.rename(newRoot_, bundlePath_)", mac)
        self.assertIn("dir.rename(aside, bundlePath_);", mac)
        self.assertIn("writeReplacedList(replaced)", mac)

    def test_macos_restarts_through_open(self):
        restart = body(read("updateinstaller.cpp"), "bool UpdateInstaller::restart(")
        self.assertIn('QStringLiteral("-n"), bundlePath_', restart)
        self.assertIn('QStringLiteral("--args")', restart)

    def test_built(self):
        sources = (ROOT / "sources.pri").read_text(encoding="utf-8")
        self.assertIn("src/updateinstaller.h", sources)
        self.assertIn("src/updateinstaller.cpp", sources)


if __name__ == "__main__":
    unittest.main()
