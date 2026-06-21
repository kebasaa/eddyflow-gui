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

#if defined(Q_OS_WIN)
#include <windows.h>
#include <tlhelp32.h>
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
}

bool Process::engineProcessStart(const QString& fullPath, const QString& workingDir, const QStringList& argList)
{
    connect(process_, SIGNAL(finished(int, QProcess::ExitStatus)),
             this, SLOT(processFinished(int, QProcess::ExitStatus)));
    connect(process_, SIGNAL(error(QProcess::ProcessError)),
             this, SLOT(processError(QProcess::ProcessError)));

    process_->setWorkingDirectory(workingDir);

    // NOTE: start() function without args not parse correctly filepath with spaces, Qt bug?
    process_->start(fullPath, argList, QProcess::Unbuffered | QProcess::ReadOnly);
    processPid_ = process_->processId();

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

    connect(process_, SIGNAL(finished(int, QProcess::ExitStatus)),
             this, SLOT(processFinished(int, QProcess::ExitStatus)));
    connect(process_, SIGNAL(error(QProcess::ProcessError)),
             this, SLOT(processError(QProcess::ProcessError)));

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

    connect(process_, SIGNAL(finished(int, QProcess::ExitStatus)),
             this, SLOT(processFinished(int, QProcess::ExitStatus)));
    connect(process_, SIGNAL(error(QProcess::ProcessError)),
             this, SLOT(processError(QProcess::ProcessError)));

    process_->start(fp, args, QProcess::Unbuffered | QProcess::ReadWrite);

    if (!process_->waitForFinished())
        return false;

    QByteArray dataList = process_->readAllStandardOutput();
    return dataList.contains(filePattern.mid(1).toLatin1());
}

#if defined(Q_OS_WIN)
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
    suspendResumeProcessThreads(static_cast<DWORD>(processPid_), true);
#elif defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    QProcess::startDetached(QStringLiteral("kill"),
                            {QStringLiteral("-STOP"), QString::number(processPid_)});
#endif
}

void Process::processResume(Defs::CurrRunStatus mode)
{
    Q_UNUSED(mode)
#if defined(Q_OS_WIN)
    suspendResumeProcessThreads(static_cast<DWORD>(processPid_), false);
#elif defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    QProcess::startDetached(QStringLiteral("kill"),
                            {QStringLiteral("-CONT"), QString::number(processPid_)});
#endif
}

void Process::processStop()
{
    // to avoid crash message error in windows
    disconnect(process_, SIGNAL(finished(int, QProcess::ExitStatus)),
             this, SLOT(processFinished(int, QProcess::ExitStatus)));
    disconnect(process_, SIGNAL(error(QProcess::ProcessError)),
             this, SLOT(processError(QProcess::ProcessError)));

    process_->kill();
    processExit_ = ExitStatus::Stopped;
}

void Process::processError(QProcess::ProcessError error)
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
    disconnect(process_, SIGNAL(error(QProcess::ProcessError)),
             this, SLOT(processError(QProcess::ProcessError)));
    emit processFailure();
}

void Process::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // to avoid multiple call
    disconnect(process_, SIGNAL(finished(int, QProcess::ExitStatus)),
             this, SLOT(processFinished(int, QProcess::ExitStatus)));

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
        env.insert(keyValue.split(QStringLiteral("=")).first(),
                   keyValue.split(QStringLiteral("=")).last());
    }

    process_->setProcessEnvironment(env);
}


