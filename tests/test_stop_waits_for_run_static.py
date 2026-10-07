"""Stop waits for the run's processes to exit before the tmp folder is wiped.

EddyPro, which this interface comes from, wipes and recreates the whole env
tmp folder after a user Stop (stopEngineProcess -> cleanEnvTmpDir) and does
nothing else with it; the engine removes its own run folder only when a run
finishes cleanly. EddyFlow keeps exactly that.

What changed is how a run is stopped: the whole job object is terminated, a
dozen processes in a parallel run, and termination only starts them dying.
The wipe ran straight after, met files the dying processes still held open,
and left the stopped run's tmp_<timestamp> folders behind.

Stop now opens a handle on every member of the job before terminating it and
waits on those handles - five seconds at most. The job's own count of active
processes is not enough: measured, it reached zero within milliseconds while
the wipe after it still found files held. And the wipe is retried for up to
three seconds while anything is left, for a file Windows or a virus scanner
holds a moment longer.
"""

import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]


def read(rel):
    return (GUI_ROOT / rel).read_text(encoding="utf-8", errors="replace")


PROCESS = read("src/process.cpp")
HEADER = read("src/process.h")
MAIN = read("src/mainwindow.cpp")


def body(source, signature):
    i = source.index(signature)
    return source[i:source.index("\n}\n", i)]


class StopWaitsForTheRun(unittest.TestCase):

    def test_handles_are_opened_before_the_job_is_terminated(self):
        stop = body(PROCESS, "void Process::processStop()")
        self.assertLess(stop.index("OpenProcess(SYNCHRONIZE, FALSE, id)"),
                        stop.index("TerminateJobObject"))
        self.assertLess(stop.index("TerminateJobObject"),
                        stop.index("waitForProcessesToExit(members, 5000)"))
        self.assertLess(stop.index("waitForProcessesToExit(members, 5000)"),
                        stop.index("releaseRunGroup();"))

    def test_each_process_is_waited_on_within_one_deadline(self):
        wait = body(PROCESS, "static void waitForProcessesToExit(")
        self.assertIn("WaitForSingleObject(h, left)", wait)
        self.assertIn("CloseHandle(h)", wait)

    def test_posix_waits_for_the_group(self):
        wait = body(PROCESS, "void Process::waitForRunGroupToGo()")
        self.assertIn("::kill(-static_cast<pid_t>(processPid_), 0)", wait)
        self.assertIn("void waitForRunGroupToGo();", HEADER)


class TheTmpFolderIsHandledAsEddyProHandlesIt(unittest.TestCase):

    def test_stop_still_wipes_the_env_tmp_folder_after_the_run_has_gone(self):
        stop = body(MAIN, "void MainWindow::stopEngineProcess()")
        self.assertLess(stop.index("processStop()"), stop.index("cleanEnvTmpDir()"))

    def test_the_wipe_is_retried_while_anything_is_left(self):
        clean = body(MAIN, "void MainWindow::cleanEnvTmpDir()")
        self.assertIn("Defs::TMP_FILE_DIR", clean)
        self.assertIn("FileUtils::cleanDirRecursively(tmpDir)", clean)
        self.assertIn("QDir(tmpDir).isEmpty() || clock.elapsed() > 3000", clean)


if __name__ == "__main__":
    unittest.main()
