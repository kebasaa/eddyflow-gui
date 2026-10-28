/***************************************************************************
  updateinstaller.cpp
  -------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "updateinstaller.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QtConcurrentRun>

#include <climits>

#include "JlCompress.h"
#include "defs.h"

namespace
{
const auto OLD_SUFFIX = QStringLiteral(".old");
const auto REPLACED_LIST = QStringLiteral("replaced.txt");
const auto EXTRACTED_DIR = QStringLiteral("new");

QString programDir()
{
    return QCoreApplication::applicationDirPath();
}

QString sevenZip()
{
    return programDir() + QLatin1Char('/') + Defs::BIN_FILE_DIR
           + QLatin1Char('/') + Defs::COMPRESSOR_BIN;
}

QString replacedList()
{
    return UpdateInstaller::stagingDir() + QLatin1Char('/') + REPLACED_LIST;
}

[[maybe_unused]] QStringList readReplacedList()
{
    QFile list(replacedList());
    if (!list.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QStringList lines;
    for (const auto& line : QString::fromUtf8(list.readAll()).split(QLatin1Char('\n')))
    {
        if (!line.trimmed().isEmpty())
            lines << line.trimmed();
    }
    return lines;
}

[[maybe_unused]] bool writeReplacedList(const QStringList& lines)
{
    QSaveFile list(replacedList());
    if (!list.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    list.write((lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8());
    return list.commit();
}

// everything in the staging folder but the list of files still to remove,
// and the folder itself once nothing is left in it
void removeStaged()
{
    QDir staging(UpdateInstaller::stagingDir());
    if (!staging.exists())
        return;
    const auto entries = staging.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot
                                               | QDir::Hidden | QDir::System);
    for (const auto& entry : entries)
    {
        if (entry.fileName() == REPLACED_LIST)
            continue;
        if (entry.isDir())
            QDir(entry.absoluteFilePath()).removeRecursively();
        else
            QFile::remove(entry.absoluteFilePath());
    }
    if (staging.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
        staging.removeRecursively();
}

// the shallowest folder under root that holds a file of this name: the
// release's own top folder, whatever it is called, or root itself
QString folderHolding(const QString& root, const QString& fileName)
{
    const QDir base(root);
    QString best;
    int bestDepth = INT_MAX;
    QDirIterator it(root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QFileInfo file(it.next());
        if (file.fileName().compare(fileName, Qt::CaseInsensitive) != 0)
            continue;
        const auto relative = base.relativeFilePath(file.absolutePath());
        const int depth = relative == QStringLiteral(".")
                              ? 0 : static_cast<int>(relative.count(QLatin1Char('/'))) + 1;
        if (depth < bestDepth)
        {
            best = file.absolutePath();
            bestDepth = depth;
        }
    }
    return best;
}
} // namespace

UpdateInstaller::UpdateInstaller(QObject* parent) :
    QObject(parent),
    // before anything is renamed: the program to start again
    programPath_(QCoreApplication::applicationFilePath()),
    process_(new QProcess(this)),
    extracting_(false)
{
    process_->setStandardOutputFile(QProcess::nullDevice());
    process_->setStandardErrorFile(QProcess::nullDevice());
    connect(process_, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status)
    {
        finishExtraction(status == QProcess::NormalExit && exitCode == 0,
                         tr("7-Zip could not extract the release (exit code %1).").arg(exitCode));
    });
    connect(process_, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error)
    {
        // a crash also ends in finished(); only a failed start does not
        if (error == QProcess::FailedToStart)
            finishExtraction(false, tr("Could not start %1.").arg(QDir::toNativeSeparators(sevenZip())));
    });
    connect(&zipWatcher_, &QFutureWatcher<QStringList>::finished, this, [this]()
    {
        finishExtraction(!zipWatcher_.result().isEmpty(),
                         tr("The release archive could not be extracted."));
    });
}

UpdateInstaller::~UpdateInstaller()
{
    if (process_->state() != QProcess::NotRunning)
    {
        process_->kill();
        process_->waitForFinished(3000);
    }
    zipWatcher_.waitForFinished();
}

bool UpdateInstaller::isAvailable()
{
#if defined(Q_OS_WIN)
    // a portable folder under Program Files, say, cannot be written to
    QTemporaryFile probe(programDir() + QStringLiteral("/update_probe_XXXXXX"));
    return probe.open();
#else
    return false;
#endif
}

bool UpdateInstaller::canExtract(const QString& archiveName)
{
    if (archiveName.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive))
        return true;
    if (archiveName.endsWith(QStringLiteral(".7z"), Qt::CaseInsensitive))
        return QFileInfo::exists(sevenZip());
    return false;
}

QString UpdateInstaller::stagingDir()
{
    return programDir() + QLatin1Char('/') + Defs::UPDATE_STAGING_DIR;
}

void UpdateInstaller::cleanUp()
{
#if defined(Q_OS_WIN)
    // nothing to do unless an update was installed
    if (!QFileInfo::exists(replacedList()))
        return;

    const QDir program(programDir());
    QStringList kept;
    for (const auto& relative : readReplacedList())
    {
        // only what an update set aside, and only inside the program folder
        if (!relative.endsWith(OLD_SUFFIX) || QDir::isAbsolutePath(relative)
            || relative.split(QLatin1Char('/')).contains(QStringLiteral("..")))
        {
            continue;
        }
        const auto path = program.filePath(relative);
        // still held by an EddyFlow that has not closed yet: next time
        if (QFileInfo::exists(path) && !QFile::remove(path))
            kept << relative;
    }

    if (kept.isEmpty())
        QFile::remove(replacedList());
    else
        writeReplacedList(kept);
    removeStaged();
#endif
}

QString UpdateInstaller::prepare(const QString& archiveName)
{
    discard();
    if (!QDir().mkpath(stagingDir()))
        return QString();
    return stagingDir() + QLatin1Char('/') + QFileInfo(archiveName).fileName();
}

bool UpdateInstaller::verify(const QString& archive, qint64 size, const QByteArray& sha256,
                             QString* error)
{
    QFile file(archive);
    if (!file.open(QIODevice::ReadOnly))
    {
        *error = file.errorString();
        return false;
    }
    if (size > 0 && file.size() != size)
    {
        *error = tr("The download is incomplete (%1 of %2 bytes).").arg(file.size()).arg(size);
        return false;
    }
    if (!sha256.isEmpty())
    {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file) || hash.result() != sha256)
        {
            *error = tr("The download does not match the checksum published with the release.");
            return false;
        }
    }
    return true;
}

void UpdateInstaller::extract(const QString& archive)
{
    newRoot_.clear();
    const QString target = stagingDir() + QLatin1Char('/') + EXTRACTED_DIR;
    QDir(target).removeRecursively();
    if (!QDir().mkpath(target))
    {
        emit extracted(false, tr("Could not create %1.").arg(QDir::toNativeSeparators(target)));
        return;
    }
    extracting_ = true;

    // a zip needs nothing but QuaZip; off the interface thread, as it takes a while
    if (archive.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive))
    {
        zipWatcher_.setFuture(QtConcurrent::run([archive, target]()
        {
            return JlCompress::extractDir(archive, target);
        }));
        return;
    }

    process_->start(sevenZip(), { QStringLiteral("x"),
                                  QDir::toNativeSeparators(archive),
                                  QStringLiteral("-o") + QDir::toNativeSeparators(target),
                                  QStringLiteral("-y") });
}

void UpdateInstaller::finishExtraction(bool ok, const QString& error)
{
    // an extraction discarded meanwhile has nothing more to say
    if (!extracting_)
        return;
    extracting_ = false;
    if (!ok)
    {
        emit extracted(false, error);
        return;
    }
    const auto program = QFileInfo(programPath_).fileName();
    newRoot_ = folderHolding(stagingDir() + QLatin1Char('/') + EXTRACTED_DIR, program);
    if (newRoot_.isEmpty())
    {
        emit extracted(false, tr("The release does not contain %1.").arg(program));
        return;
    }
    emit extracted(true, QString());
}

bool UpdateInstaller::install(QString* error)
{
#if defined(Q_OS_WIN)
    if (newRoot_.isEmpty())
    {
        *error = tr("The release has not been extracted.");
        return false;
    }

    // listed first: the files move out of the folder being walked
    QStringList sources;
    QDirIterator it(newRoot_, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext())
        sources << it.next();

    const QDir program(programDir());
    const QDir from(newRoot_);
    QStringList setAside;   // targets renamed to <name>.old
    QStringList movedIn;    // new files moved into place

    auto fail = [&](const QString& why)
    {
        // every step undone, in reverse: the old version stays as it was
        for (const auto& target : std::as_const(movedIn))
            QFile::remove(target);
        for (const auto& target : std::as_const(setAside))
            QFile::rename(target + OLD_SUFFIX, target);
        *error = why;
        return false;
    };

    for (const auto& source : std::as_const(sources))
    {
        const QString relative = from.relativeFilePath(source);
        const QString target = program.filePath(relative);
        if (!program.mkpath(QFileInfo(relative).path()))
            return fail(tr("Could not create the folder for %1.").arg(QDir::toNativeSeparators(target)));

        // a running program or a loaded library cannot be overwritten, but
        // it can be renamed
        if (QFileInfo::exists(target))
        {
            const QString aside = target + OLD_SUFFIX;
            if (QFileInfo::exists(aside) && !QFile::remove(aside))
                return fail(tr("%1 is in use.").arg(QDir::toNativeSeparators(aside)));
            if (!QFile::rename(target, aside))
                return fail(tr("%1 could not be replaced: it may be in use or read-only.")
                                .arg(QDir::toNativeSeparators(target)));
            setAside << target;
        }
        if (!QFile::rename(source, target))
            return fail(tr("%1 could not be moved into place.").arg(QDir::toNativeSeparators(target)));
        movedIn << target;
    }

    // what cleanUp() removes on the next start, along with anything an
    // earlier update could not remove yet
    QStringList replaced = readReplacedList();
    for (const auto& target : std::as_const(setAside))
    {
        const auto relative = program.relativeFilePath(target) + OLD_SUFFIX;
        if (!replaced.contains(relative))
            replaced << relative;
    }
    if (!writeReplacedList(replaced))
        qWarning() << "update: could not record the replaced files in" << replacedList();
    newRoot_.clear();
    removeStaged();
    return true;
#else
    *error = tr("EddyFlow can update itself only on Windows.");
    return false;
#endif
}

bool UpdateInstaller::restart(const QStringList& arguments) const
{
    return QProcess::startDetached(programPath_, arguments, programDir());
}

void UpdateInstaller::discard()
{
    // whatever the extraction reports after this is ignored
    extracting_ = false;
    if (process_->state() != QProcess::NotRunning)
    {
        process_->kill();
        process_->waitForFinished(3000);
    }
    // QuaZip cannot be interrupted: its files go once it is done
    zipWatcher_.waitForFinished();
    newRoot_.clear();
    removeStaged();
}
