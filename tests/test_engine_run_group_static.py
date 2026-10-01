"""Stop, Pause and Resume reach every process of an engine run.

A parallel pre-pass is one eddyflow_rp parent and up to 32 workers. Each
worker is launched through a shell script that exits as soon as it has started
it, so the workers' parent chain is broken from the first second: nothing can
find them again by walking down from the parent.

The interface used to act on the parent alone. Stop was `process_->kill()`,
and Windows does not take a killed process's children with it - measured on the
Yatir run, two minutes after Stop the parent was gone and six workers and their
six launchers were still computing, one of them writing a 154 KB dump into a
dead run's folder seven minutes later. Pause suspended only the parent's
threads, so a "paused" run kept going in every worker.

So the engine runs inside a group the operating system maintains: a job object
on Windows, a process group elsewhere. Stop terminates the group, Pause and
Resume walk it, and on Windows the job's KILL_ON_JOB_CLOSE means the interface
closing - or crashing - takes the run with it.

The engine is created suspended and released only once it is in the job, so it
cannot start a worker before the job exists to catch it.

The engine side - a worker that stops when its parent goes, which covers every
way a parent can end that the interface never sees - is in eddyflow-engine.

Part of the EddyFlow GUI's static checks.
"""

from pathlib import Path
import re
import unittest


GUI_ROOT = Path(__file__).resolve().parent.parent


def read(rel):
    return (GUI_ROOT / rel).read_text(encoding="utf-8", errors="replace")


CPP = read("src/process.cpp")
HDR = read("src/process.h")


def body(signature):
    start = CPP.index(signature)
    brace = CPP.index("{", start)
    depth, i = 0, brace
    while True:
        if CPP[i] == "{":
            depth += 1
        elif CPP[i] == "}":
            depth -= 1
            if depth == 0:
                return CPP[brace:i + 1]
        i += 1


START = body("bool Process::engineProcessStart(")
STOP = body("void Process::processStop()")
PAUSE = body("void Process::processPause(")
RESUME = body("void Process::processResume(")


class TheRunIsBornInsideItsGroup(unittest.TestCase):

    def test_the_job_kills_on_close(self):
        self.assertIn("JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE", START)

    def test_the_engine_is_created_suspended(self):
        self.assertIn("CREATE_SUSPENDED", START)

    def test_it_is_assigned_before_it_is_released(self):
        """Released first, it could start a worker the job never sees."""
        self.assertLess(START.index("AssignProcessToJobObject"),
                        START.index("suspendResumeProcessThreads("))

    def test_it_is_released_even_if_assignment_fails(self):
        """A run outside its job still has to run, not sit suspended."""
        assign = START.index("AssignProcessToJobObject")
        release = START.index("suspendResumeProcessThreads(")
        guard = START.rindex("if (h)", 0, assign)
        block_end = START.index("}", assign)
        self.assertGreater(release, block_end,
                           "the release must not sit inside the if (h) block")
        self.assertLess(guard, assign)

    def test_unix_gets_its_own_process_group(self):
        self.assertIn("setChildProcessModifier", START)
        self.assertIn("setsid", START)


class StopReachesEveryProcess(unittest.TestCase):

    def test_the_group_is_terminated_not_just_the_parent(self):
        self.assertIn("TerminateJobObject", STOP)
        self.assertIn("SIGKILL", STOP)
        self.assertRegex(STOP, r"::kill\(\s*-static_cast<pid_t>\(processPid_\)")

    def test_the_parent_is_still_killed_for_qprocess(self):
        """QProcess still has to see its own child end."""
        self.assertIn("process_->kill()", STOP)


class PauseAndResumeReachEveryProcess(unittest.TestCase):

    def test_windows_walks_the_job(self):
        self.assertIn("suspendResumeRun(", PAUSE)
        self.assertIn("suspendResumeRun(", RESUME)
        self.assertIn("JobObjectBasicProcessIdList", CPP)

    def test_the_parent_is_suspended_first_and_resumed_last(self):
        """Otherwise it can start a worker between the listing and the
        suspension, and that worker runs on while the rest are paused."""
        run = body("static void suspendResumeRun(")
        self.assertLess(run.index("if (suspend) suspendResumeProcessThreads(parent, true)"),
                        run.index("for (DWORD id : jobProcessIds(job))"))
        self.assertLess(run.index("for (DWORD id : jobProcessIds(job))"),
                        run.index("if (!suspend) suspendResumeProcessThreads(parent, false)"))

    def test_unix_signals_the_group(self):
        self.assertIn("SIGSTOP", PAUSE)
        self.assertIn("SIGCONT", RESUME)
        self.assertNotIn("startDetached", PAUSE + RESUME)


class NothingOfAFinishedRunLingers(unittest.TestCase):

    def test_the_job_is_released_when_the_run_ends(self):
        self.assertIn("releaseRunGroup();", body("void Process::processFinished("))

    def test_and_before_the_next_one_starts(self):
        self.assertLess(START.index("releaseRunGroup();"), START.index("process_->start("))

    def test_and_when_the_object_goes(self):
        self.assertIn("releaseRunGroup();", body("Process::~Process()"))

    def test_the_header_keeps_windows_types_out(self):
        self.assertIn("void* job_ = nullptr;", HDR)
        #> The include, not the word - the comment beside job_ says why the
        #> header avoids it.
        self.assertNotRegex(HDR, r"#\s*include\s*<windows\.h>")


if __name__ == "__main__":
    unittest.main()
