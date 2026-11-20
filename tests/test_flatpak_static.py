"""The Flatpak manifest puts every file where the GUI and the engine look.

The GUI runs the engine from applicationDirPath()/bin (Defs::BIN_FILE_DIR) and
7-Zip from the same folder (Defs::COMPRESSOR_BIN); the engine itself calls
7za from the PATH on Linux (set_os_environment.f90). A manifest that put any
of them elsewhere would build, install and start - and then fail at the first
run. .github/workflows/linux-flatpak.yml builds the bundle and checks the
same paths inside the sandbox.
"""

from pathlib import Path
import unittest

GUI_ROOT = Path(__file__).resolve().parents[1]
MANIFEST = GUI_ROOT / "packaging" / "flatpak" / "io.github.kebasaa.EddyFlow.yml"
WORKFLOW = GUI_ROOT / ".github" / "workflows" / "linux-flatpak.yml"


def read(p):
    return p.read_text(encoding="utf-8", errors="replace")


class TheManifest(unittest.TestCase):
    def setUp(self):
        self.m = read(MANIFEST)
        self.defs = read(GUI_ROOT / "src" / "defs.h")

    def test_engine_where_the_gui_looks(self):
        self.assertIn('BIN_FILE_DIR         = QStringLiteral("bin")', self.defs)
        self.assertIn("/app/lib/eddyflow/bin/eddyflow_rp", self.m)
        self.assertIn("/app/lib/eddyflow/bin/eddyflow_fcc", self.m)
        self.assertIn("/app/lib/eddyflow/eddyflow", self.m)

    def test_7zip_for_the_gui_and_for_the_engine(self):
        self.assertIn('COMPRESSOR_BIN        = QStringLiteral("7z%1")', self.defs)
        self.assertIn("/app/lib/eddyflow/bin/7z", self.m)
        self.assertIn("ln -s 7zz /app/bin/7za", self.m)

    def test_desktop_metainfo_and_icon_share_the_app_id(self):
        app_id = "io.github.kebasaa.EddyFlow"
        self.assertIn(f"app-id: {app_id}", self.m)
        for suffix in (".desktop", ".metainfo.xml"):
            self.assertTrue((MANIFEST.parent / f"{app_id}{suffix}").is_file())
        self.assertIn(f"apps/{app_id}.png", self.m)

    def test_qt_new_enough(self):
        """CMakeLists asks for Qt 6.7; the KDE 6.11 runtime carries Qt 6.11.
        KDE 6.8 went end-of-life and its image could no longer build."""
        self.assertIn("runtime-version: '6.11'", self.m)
        self.assertIn("find_package(Qt6 6.7", read(GUI_ROOT / "CMakeLists.txt"))


class TheWorkflow(unittest.TestCase):
    def setUp(self):
        self.w = read(WORKFLOW)

    def test_same_kde_image_as_the_runtime(self):
        import re
        version = re.search(r"runtime-version: '([0-9.]+)'", read(MANIFEST)).group(1)
        self.assertIn(f"flatpak-github-actions:kde-{version}\n", self.w,
                      "the image must carry the runtime the manifest names")
        self.assertIn("options: --privileged", self.w)

    def test_an_older_tag_is_built_with_the_current_packaging(self):
        """So a release whose packaging stopped working can be rebuilt by hand
        without moving its tag. The copy comes before the build."""
        copy = self.w.index("cp -R packaging-current/packaging/flatpak/. "
                            "eddyflow-gui/packaging/flatpak/")
        self.assertLess(self.w.index("path: packaging-current"), copy)
        self.assertLess(copy, self.w.index("flatpak/flatpak-github-actions/flatpak-builder@"))

    def test_engine_and_7zip_checked_inside_the_sandbox(self):
        self.assertIn("--command=/app/lib/eddyflow/bin/eddyflow_rp", self.w)
        self.assertIn("command -v 7za", self.w)

    def test_attached_to_the_release(self):
        self.assertIn("gh release upload", self.w)


if __name__ == "__main__":
    unittest.main()
