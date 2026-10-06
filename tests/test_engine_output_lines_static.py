"""Engine output reaches the console and the warning panel one whole line at a time.

Two faults, both seen in a real run:

- A message raised while the engine had a progress line open was written onto
  it: "   Absolute limits test.. Warning(109)> One or more gases ...". The
  progress keywords are matched first and return, so that line never reached
  the warning panel and the warning lost its first line. The line is now split
  at the message tag and handed on as two.
- The tail of each read after its last newline was handled at once and kept,
  so a line a pipe read happened to cut was shown twice, the first time cut
  short. Only complete lines are handled now; the rest waits for its newline,
  and whatever is left when the run ends is still shown.
"""

import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]


def read(rel):
    return (GUI_ROOT / rel).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


RUNPAGE = read("src/runpage.cpp")
HEADER = read("src/runpage.h")
MAIN = read("src/mainwindow.cpp")


def body(source, signature):
    i = source.index(signature)
    return source[i:source.index("\n}\n", i)]


class OnlyCompleteLines(unittest.TestCase):

    def test_the_text_after_the_last_newline_waits(self):
        buf = body(RUNPAGE, "void RunPage::bufferData(QByteArray &data)")
        self.assertIn("rxBuffer_.lastIndexOf('\\n')", buf)
        self.assertIn("rxBuffer_ = rxBuffer_.mid(lastNewline + 1);", buf)
        self.assertLess(buf.index("if (lastNewline < 0)"), buf.index("split('\\n')"))

    def test_what_is_left_at_the_end_of_a_run_is_shown(self):
        flush = body(RUNPAGE, "void RunPage::flushBuffer()")
        self.assertIn("bufferData(end);", flush)
        self.assertIn("void flushBuffer();", HEADER)
        self.assertEqual(MAIN.count("&RunPage::flushBuffer"), 2)
        self.assertNotIn("&RunPage::resetBuffer", MAIN)


class AMessageGluedToAProgressLineIsSplitOff(unittest.TestCase):

    def test_every_line_goes_through_the_split(self):
        buf = body(RUNPAGE, "void RunPage::bufferData(QByteArray &data)")
        self.assertIn("splitGluedMessage(rawLine)", buf)
        self.assertLess(buf.index("splitGluedMessage(rawLine)"),
                        buf.index("parseEngineOutput(data);"))

    def test_the_split_is_at_the_engine_s_message_tags(self):
        split = body(RUNPAGE, "QByteArrayList RunPage::splitGluedMessage(")
        self.assertIn("(?:Fatal error|Warning|Error|Alert)\\\\(\\\\d+\\\\)>", split)
        self.assertIn("match.capturedStart() > 0", split)
        self.assertIn("!before.trimmed().isEmpty()", split)


if __name__ == "__main__":
    unittest.main()
