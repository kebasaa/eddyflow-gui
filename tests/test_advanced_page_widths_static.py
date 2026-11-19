"""The Advanced Settings pages fit a 1920 px laptop screen at 125 %.

That is 1536 logical px. Measured with the layout report (layoutreport.h:
EDDYFLOW_LAYOUT_REPORT=<file>, offscreen, 8- and 80-gas projects), four views
scrolled sideways, each because of what sat side by side in one grid:

  Processing Options     1547 px for 1218 - two time-lag buttons in two
                         columns, the wind offsets widened 1.3x, and long
                         option labels that could not wrap. Now 1048.
  Statistics, spikes     1213 px for 958 - the consecutive-difference limits
                         in a second column pair. Now 829.
  Statistics, abs. lim.  962 px for 958 - a 320 px picture beside two wide
                         spin boxes per gas. Now 922.
  Output Files           1272 px for 1218 - a two-line checkbox label whose
                         longer line set its column. Now 1209.

Pinned: each of those causes stays fixed, and the report stays a no-op
unless its variable is set.
"""

import re
import unittest
from pathlib import Path

SRC = Path(__file__).resolve().parents[1] / "src"


def read(name):
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


PROC = read("advprocessingoptions.cpp")
STATS = read("advstatisticaloptions.cpp")
OUTPUT = read("advoutputoptions.cpp")
MAIN = read("mainwindow.cpp")
APP = read("main.cpp")
RICH = read("richtextcheckbox.cpp")


class ProcessingOptions(unittest.TestCase):

    def test_the_time_lag_buttons_share_one_column(self):
        self.assertIn("settingsLayout->addLayout(tlButtons, 13, 3);", PROC)
        self.assertNotIn("tlPreassessButton, 13, 4", PROC)

    def test_the_wind_offsets_are_not_widened_on_windows(self):
        self.assertNotIn("sizeHint().width() * 1.3", PROC)

    def test_option_labels_wrap_once_they_can_be_found(self):
        wrap = PROC.index("box->setWordWrap(true);")
        self.assertLess(PROC.index("getContainerScrollArea(this, settingsLayout)"), wrap,
                        "before the layout is on a widget, findChildren finds none")
        self.assertIn("label->setWordWrap(on);", RICH)


class StatisticalAnalysis(unittest.TestCase):

    def test_the_step_limits_sit_below_the_sigma_table(self):
        #> Columns 2 and 3, placed after "All other variables", not a pair of
        #> their own at columns 4 and 5.
        self.assertIn("tab0Grid_->addWidget(stepDespikingRadio, stepRow++, 2, 1, 2);", STATS)
        self.assertNotIn("stepDespikingRadio, 0, 4", STATS)
        self.assertNotIn("stepRow, 4", STATS)
        self.assertLess(STATS.index("tab0Grid_->addWidget(despSpin_2, srRow, 3);"),
                        STATS.index("int stepRow = srRow + 1;"))

    def test_the_test_pictures_are_at_most_280_px(self):
        self.assertIn("full.scaledToWidth(280, Qt::SmoothTransformation)", STATS)
        self.assertEqual(len(re.findall(r"GraphLabel->setPixmap\(testGraph\(", STATS)), 9)
        self.assertNotRegex(STATS, r'GraphLabel->setPixmap\(QPixmap\(QStringLiteral\(":/icons/[a-z]+"\)\)\)')


class OutputFiles(unittest.TestCase):

    def test_the_continuous_dataset_label_is_three_short_lines(self):
        m = re.search(r'createDatasetCheckBox->setText\(tr\((.*?)\)\);', OUTPUT, re.S)
        text = "".join(re.findall(r'"([^"]*)"', m.group(1)))
        lines = text.split("\\n")
        self.assertEqual(len(lines), 3)
        self.assertLessEqual(max(len(line) for line in lines), 47)


class TheReport(unittest.TestCase):

    def test_it_does_nothing_unless_asked(self):
        fn = MAIN[MAIN.index("void MainWindow::scheduleLayoutReport()"):]
        fn = fn[:fn.index("\n}\n")]
        self.assertIn('qEnvironmentVariable("EDDYFLOW_LAYOUT_REPORT")', fn)
        self.assertIn("if (path.isEmpty()) { return; }", fn)
        self.assertIn("mainWin.scheduleLayoutReport();", APP)

    def test_it_keeps_the_users_settings_out_of_it(self):
        guard = APP[APP.index('if (!qEnvironmentVariable("EDDYFLOW_LAYOUT_REPORT").isEmpty())'):]
        guard = guard[:guard.index("}")]
        self.assertIn("QSettings::setDefaultFormat(QSettings::IniFormat);", guard)
        self.assertIn("layout-report-settings", guard)
        self.assertLess(APP.index("QSettings::setDefaultFormat"), APP.index("MainWindow mainWin("))


if __name__ == "__main__":
    unittest.main()
