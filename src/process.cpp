/***************************************************************************
  process.cpp
  -------------------
  Copyright © 2007-2011, Eco2s team, Antonio Forgione
  Copyright © 2011-2018, LI-COR Biosciences, Antonio Forgione
  Copyright © 2026,      ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.

  EddyFlow® contains additional Open Source Components. The licenses
  and/or notices these Components can be found in the file LIBRARIES.txt.

  EddyFlow® is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
****************************************************************************/

#include "process.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include <vector>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <tlhelp32.h>
#elif defined(Q_OS_UNIX)
#include <signal.h>
#include <unistd.h>
#endif

#if defined(Q_OS_WIN)
static void suspendResumeProcessThreads(DWORD pid, bool suspend);
#endif


Process::Process(QObject* parent, const QString &fullPath) :
    QObject(parent),
    process_(nullptr),
    fullPath_(fullPath),
    processExit_(ExitStatus::Success),
    processPid_(0)
{
    process_ = new QProcess(this);
    connect(process_, &QProcess::readyReadStandardOutput,
             this, &Process::readyReadStdOut);
    connect(process_, &QProcess::readyReadStandardError,
             this, &Process::readyReadStdErr);

}

Process::~Process()
{
    releaseRunGroup();
}

void Process::releaseRunGroup()
{
#if defined(Q_OS_WIN)
    if (job_)
    {
        // KILL_ON_JOB_CLOSE: whatever of the finished run is still alive -
        // a worker, a launcher, an archive being decompressed ahead - goes
        // with the handle.
        CloseHandle(static_cast<HANDLE>(job_));
        job_ = nullptr;
    }
#endif
}

bool Process::engineProcessStart(const QString& fullPath, const QString& workingDir, const QStringList& argList)
{
    connect(process_, &QProcess::finished,
             this, &Process::processFinished);
    connect(process_, &QProcess::errorOccurred,
             this, &Process::onProcessError);

    process_->setWorkingDirectory(workingDir);

    // Everything the engine starts must stop when it is stopped. A parallel
    // pre-pass is one parent and up to 32 workers, each launched through a
    // shell script that exits at once - so the workers' parent chain is
    // broken from the start, and nothing short of a group the OS maintains
    // can find them all again. Killing the parent alone left six workers
    // computing for hours on the Yatir run.
    releaseRunGroup();
#if defined(Q_OS_WIN)
    // A job with KILL_ON_JOB_CLOSE: TerminateJobObject on Stop, and the
    // handle closing - this object destroyed, or the interface exiting or
    // crashing - takes every process in it.
    //
    // The engine is created suspended and only released once it is in the
    // job, so it cannot start a child before the job exists to catch it.
    job_ = CreateJobObjectW(nullptr, nullptr);
    if (job_)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(static_cast<HANDLE>(job_), JobObjectExtendedLimitInformation,
                                &info, sizeof(info));
        process_->setCreateProcessArgumentsModifier(
            [](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_SUSPENDED; });
    }
    else
    {
        process_->setCreateProcessArgumentsModifier({});
    }
#elif defined(Q_OS_UNIX)
    // Its own session, so its own process group with the engine as leader.
    // Workers start through `sh script &` and inherit the group.
    process_->setChildProcessModifier([] { ::setsid(); });
#endif

    // NOTE: start() function without args not parse correctly filepath with spaces, Qt bug?
    process_->start(fullPath, argList, QProcess::Unbuffered | QProcess::ReadOnly);
    processPid_ = process_->processId();

#if defined(Q_OS_WIN)
    if (job_ && processPid_ > 0)
    {
        HANDLE h = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
                               static_cast<DWORD>(processPid_));
        if (h)
        {
            AssignProcessToJobObject(static_cast<HANDLE>(job_), h);
            CloseHandle(h);
        }
        // Released whether or not the assignment took: a run outside its job
        // still has to run. Resuming every thread rather than Qt's own handle
        // to the main one, which it does not expose - and resuming a thread
        // that is already running is harmless.
        suspendResumeProcessThreads(static_cast<DWORD>(processPid_), false);
    }
#endif

    return true;
}

// add file to an archive fileName
// using an external helper (7z)
// NOTE: never used
bool Process::zipProcessAddStart(const QString &fileName,
                                 const QString &toArchive,
                                 const QString &workingDir,
                                 const QString &fileType)
{
    QString zipFileName;
    QString workDir;
    QStringList args;

    QFileInfo fileInfo(fileName);

    if (fileType.isEmpty())
    {
        zipFileName = fileName;
    }
    else
    {
        zipFileName = fileInfo.completeBaseName() + QStringLiteral(".") + fileType;
    }

    if (workingDir.isEmpty())
    {
        workDir = fileInfo.absolutePath();
    }
    else
    {
        workDir = workingDir;
    }

    // switch 'a', i.e. add
    args << QStringLiteral("a");

    // assume zip if not present
    if (fileType.isEmpty())
    {
        args << QStringLiteral("-tzip");
    }
    else
    {
        args << QStringLiteral("-t") + fileType;
    }

    args << QStringLiteral("-mx=9");
    args << zipFileName;
    args << toArchive;

    // file path of the program
    QString fp(fullPath_
            + QLatin1Char('/')
            + Defs::BIN_FILE_DIR
            + QLatin1Char('/')
            + Defs::COMPRESSOR_BIN);

    connect(process_, &QProcess::finished,
             this, &Process::processFinished);
    connect(process_, &QProcess::errorOccurred,
             this, &Process::onProcessError);

    process_->setWorkingDirectory(workDir);

    process_->start(fp, args, QProcess::Unbuffered | QProcess::ReadWrite);

    if (!process_->waitForFinished(2000))
        return false;

    return true;
}

// extract int the outDir all the metadata files in the fileName archive
// using an external helper (7z)
// NOTE: never used
bool Process::zipProcessExtMdStart(const QString& fileName, const QString& outDir)
{
    QStringList args;

    args << QStringLiteral("e");
    if (!outDir.isEmpty())
        args << QStringLiteral("-o") + outDir;
    args << fileName;
    args << QStringLiteral("*.metadata");
    args << QStringLiteral("-y");

    // file path of the program
    QString fp(fullPath_
            + QLatin1Char('/')
            + Defs::BIN_FILE_DIR
            + QLatin1Char('/')
            + Defs::COMPRESSOR_BIN);

    process_->start(fp, args, QProcess::Unbuffered | QProcess::ReadWrite);

    if (!process_->waitForFinished(2000))
        return false;

    return process_->exitCode();
}

// return if a specific file type is present in the archive
// using an external helper (7z)
// NOTE: never used
bool Process::zipContainsFiletype(const QString& fileName, const QString& filePattern)
{
    QStringList args;

    args << QStringLiteral("l");
    args << fileName;
    if (!filePattern.isEmpty())
        args << QStringLiteral("-i!") + filePattern;
    else
        args << QStringLiteral("*");

    // file path of the 7z utility
    QString fp(qApp->applicationDirPath() + QLatin1Char('/') + Defs::BIN_FILE_DIR + QLatin1Char('/') + Defs::COMPRESSOR_BIN);

    connect(process_, &QProcess::finished,
             this, &Process::processFinished);
    connect(process_, &QProcess::errorOccurred,
             this, &Process::onProcessError);

    process_->start(fp, args, QProcess::Unbuffered | QProcess::ReadWrite);

    if (!process_->waitForFinished())
        return false;

    QByteArray dataList = process_->readAllStandardOutput();
    return dataList.contains(filePattern.mid(1).toLatin1());
}

#if defined(Q_OS_WIN)
//! Every process currently in the job; empty if there is no job or the query
//! fails.
static std::vector<DWORD> jobProcessIds(HANDLE job)
{
    std::vector<DWORD> ids;
    if (!job) return ids;
    // Comfortably more than the engine can start: one parent, up to 32
    // workers and their launchers, and an archive being unpacked ahead.
    const DWORD room = 512;
    std::vector<char> buf(sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) + room * sizeof(ULONG_PTR));
    auto *list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST *>(buf.data());
    list->NumberOfAssignedProcesses = room;
    if (!QueryInformationJobObject(job, JobObjectBasicProcessIdList, list,
                                   static_cast<DWORD>(buf.size()), nullptr))
        return ids;
    for (DWORD i = 0; i < list->NumberOfProcessIdsInList; ++i)
        ids.push_back(static_cast<DWORD>(list->ProcessIdList[i]));
    return ids;
}

//! Wait until every process behind these handles has exited, limitMs in all
//! at most, and close the handles.
static void waitForProcessesToExit(std::vector<HANDLE>& handles, DWORD limitMs)
{
    const ULONGLONG deadline = GetTickCount64() + limitMs;
    for (HANDLE h : handles) {
        const ULONGLONG now = GetTickCount64();
        const DWORD left = now < deadline ? static_cast<DWORD>(deadline - now) : 0;
        WaitForSingleObject(h, left);
        CloseHandle(h);
    }
    handles.clear();
}

//! Suspend or resume a whole run. The parent goes first on suspend, so it
//! cannot start another worker between the listing and the suspension, and
//! last on resume.
static void suspendResumeRun(HANDLE job, DWORD parent, bool suspend)
{
    if (suspend) suspendResumeProcessThreads(parent, true);
    for (DWORD id : jobProcessIds(job))
        if (id != parent) suspendResumeProcessThreads(id, suspend);
    if (!suspend) suspendResumeProcessThreads(parent, false);
}

static void suspendResumeProcessThreads(DWORD pid, bool suspend)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te;
    te.dwSize = sizeof(THREADENTRY32);
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                HANDLE t = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (t) { suspend ? SuspendThread(t) : ResumeThread(t); CloseHandle(t); }
            }
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}
#endif

void Process::processPause(Defs::CurrRunStatus mode)
{
    Q_UNUSED(mode)
#if defined(Q_OS_WIN)
    suspendResumeRun(static_cast<HANDLE>(job_), static_cast<DWORD>(processPid_), true);
#elif defined(Q_OS_UNIX)
    // The whole group; the engine alone if it somehow is not leading one.
    if (processPid_ > 0 && ::kill(-static_cast<pid_t>(processPid_), SIGSTOP) != 0)
        ::kill(static_cast<pid_t>(processPid_), SIGSTOP);
#endif
}

void Process::processResume(Defs::CurrRunStatus mode)
{
    Q_UNUSED(mode)
#if defined(Q_OS_WIN)
    suspendResumeRun(static_cast<HANDLE>(job_), static_cast<DWORD>(processPid_), false);
#elif defined(Q_OS_UNIX)
    if (processPid_ > 0 && ::kill(-static_cast<pid_t>(processPid_), SIGCONT) != 0)
        ::kill(static_cast<pid_t>(processPid_), SIGCONT);
#endif
}

void Process::processStop()
{
    // to avoid crash message error in windows
    disconnect(process_, &QProcess::finished,
             this, &Process::processFinished);
    disconnect(process_, &QProcess::errorOccurred,
             this, &Process::onProcessError);

    // The whole run, not just the parent: its workers are separate
    // processes and outlive it otherwise.
#if defined(Q_OS_WIN)
    // A handle on every member before the job is terminated, to wait on
    // afterwards - see waitForRunGroupToGo. Opened now: once a process has
    // gone its id can no longer be opened, and could even be someone else's.
    std::vector<HANDLE> members;
    if (job_) {
        for (DWORD id : jobProcessIds(static_cast<HANDLE>(job_))) {
            HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, id);
            if (h) members.push_back(h);
        }
        TerminateJobObject(static_cast<HANDLE>(job_), 1);
    }
#elif defined(Q_OS_UNIX)
    if (processPid_ > 0)
        ::kill(-static_cast<pid_t>(processPid_), SIGKILL);
#endif
    process_->kill();
#if defined(Q_OS_WIN)
    waitForProcessesToExit(members, 5000);
#else
    waitForRunGroupToGo();
#endif
    releaseRunGroup();
    processExit_ = ExitStatus::Stopped;
}

//! Return once every process of the stopped run has exited, or after five
//! seconds whatever happens.
//!
//! Terminating a job, or signalling a group, only starts the processes
//! dying. The caller wipes the env tmp folder straight after Stop, as
//! EddyPro always did (stopEngineProcess -> cleanEnvTmpDir), and a file a
//! dying process still holds open cannot be deleted - so that wipe used to
//! leave a stopped run's tmp_<timestamp> folders behind, and with parallel
//! runs there are a dozen processes to wait for rather than one.
//!
//! On Windows the job's own count of active processes is no use for this:
//! it reached zero within milliseconds of the termination while the wipe
//! after it still found files held. What a process object signals is that
//! it has exited completely; see waitForProcessesToExit.
void Process::waitForRunGroupToGo()
{
#if defined(Q_OS_UNIX)
    const int limitMs = 5000;
    const int stepMs = 50;
    if (processPid_ <= 0) return;
    // The engine first, so it is reaped and no longer counts as a member.
    process_->waitForFinished(limitMs);
    for (int waited = 0; waited < limitMs; waited += stepMs) {
        if (::kill(-static_cast<pid_t>(processPid_), 0) != 0) return;
        ::usleep(stepMs * 1000);
    }
#endif
}

void Process::onProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart)
    {
        qWarning() << tr("program not found.");
        processExit_ = ExitStatus::FailureToStart;
    }
    else
    {
        processExit_ = ExitStatus::Error;
    }
    // to avoid multiple call
    disconnect(process_, &QProcess::errorOccurred,
             this, &Process::onProcessError);
    emit processFailure();
}

void Process::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // to avoid multiple call
    disconnect(process_, &QProcess::finished,
             this, &Process::processFinished);

    // Anything the run left behind - a worker that has not noticed yet, an
    // archive still being unpacked - goes now, not when the next run starts.
    releaseRunGroup();

    if (exitStatus == QProcess::CrashExit)
    {
        processExit_ = ExitStatus::Error;
        qWarning() << tr("process crashed.");
    }
    else if (exitCode != 0)
    {
        processExit_ = ExitStatus::Failure;
        qWarning() << tr("process failed.");
    }
    else
    {
        processExit_ = ExitStatus::Success;
        qWarning() << tr("process ok.");
        emit processSuccess();
        return;
    }
    emit processFailure();
}

QByteArray Process::readAllStdOut() const
{
    return process_->readAllStandardOutput();
}

QByteArray Process::readAllStdErr() const
{
    return process_->readAllStandardError();
}

QProcess *Process::process() const
{
    return process_;
}

void Process::setChannelsMode(QProcess::ProcessChannelMode mode)
{
    process_->setProcessChannelMode( mode);
}

void Process::setReadChannels(QProcess::ProcessChannel channel)
{
    process_->setReadChannel(channel);
}

void Process::setEnv(const QStringList &envList)
{
    QProcessEnvironment env;
    for (const auto &keyValue : envList)
    {
        const int sep = keyValue.indexOf(QLatin1Char('='));
        if (sep > 0)
            env.insert(keyValue.left(sep), keyValue.mid(sep + 1));
        // sep <= 0: no '=' present, or leading '=' (Windows internal drive vars) — skip
    }
    process_->setProcessEnvironment(env);
}


