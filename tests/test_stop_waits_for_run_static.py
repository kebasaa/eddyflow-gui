"""Stop waits for the run's processes to exit, then removes what is theirs.

EddyPro, which this interface comes from, wipes and recreates the whole env
tmp folder after a user Stop (stopEngineProcess -> cleanEnvTmpDir); the engine
removes its own run folder only when a run finishes cleanly. EddyFlow did the
same until a second session's run lost its scripts to it: every session of a
version shares that folder, its workers finished and could never say so, and
it waited a day for them. The cleanup now removes only what no running engine
owns - each run's folder names its owner in owner.pid, each worker folder its
parent in p<pid>_ - which for a run on its own is still everything.

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
FILEUTILS = read("src/fileutils.cpp")
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

    def test_only_what_no_running_engine_owns_is_removed(self):
        env = body(MAIN, "void MainWindow::cleanEnvTmpDir()")
        self.assertIn("FileUtils::cleanTmpDirOfFinishedRuns(appEnvPath_ + QLatin1Char('/') + Defs::TMP_FILE_DIR)", env)
        clean = body(FILEUTILS, "void FileUtils::cleanTmpDirOfFinishedRuns(")
        self.assertIn("if (!tmpEntryInUse(entry)) doomed << entry.absoluteFilePath();", clean)
        self.assertNotIn("cleanDirRecursively", clean)

    def test_the_chosen_entries_are_retried_for_three_seconds(self):
        clean = body(FILEUTILS, "void FileUtils::cleanTmpDirOfFinishedRuns(")
        self.assertIn("if (doomed.isEmpty() || clock.elapsed() > 3000) break;", clean)
        self.assertIn("if (!QFileInfo::exists(entry.absoluteFilePath())) doomed.removeAt(i);", clean)

    def test_ownership_is_read_from_the_folders(self):
        owner = body(FILEUTILS, "static bool tmpEntryInUse(")
        self.assertIn('"/owner.pid"', owner)
        self.assertIn("Process::isEngineProcessRunning(pid)", owner)
        self.assertIn(r'QStringLiteral("^p(\\d+)_")', owner)
        self.assertIn("secsTo(QDateTime::currentDateTime()) < 60", owner)

    def test_a_reused_process_id_does_not_pass_for_the_engine(self):
        alive = body(PROCESS, "bool Process::isEngineProcessRunning(qint64 pid)")
        self.assertIn("GetExitCodeProcess(h, &code) && code == STILL_ACTIVE", alive)
        self.assertIn('name.contains(QStringLiteral("eddyflow"), Qt::CaseInsensitive)', alive)


if __name__ == "__main__":
    unittest.main()
