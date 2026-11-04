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
#include <QSettings>
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
// macOS: where the replaced bundle waits for the next start, and where the
// .dmg is mounted
const auto OLD_DIR = QStringLiteral("old");
[[maybe_unused]] const auto MOUNT_DIR = QStringLiteral("mnt");

QString programDir()
{
    return QCoreApplication::applicationDirPath();
}

// the EddyFlow.app this program runs from, or empty if it is not in one
QString bundlePath()
{
    const QString bundle = QDir(programDir() + QStringLiteral("/../..")).canonicalPath();
    return bundle.endsWith(QStringLiteral(".app"), Qt::CaseInsensitive) ? bundle : QString();
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

#if defined(Q_OS_MACOS)
const auto HDIUTIL = QStringLiteral("/usr/bin/hdiutil");
const auto DITTO = QStringLiteral("/usr/bin/ditto");
const auto CODESIGN = QStringLiteral("/usr/bin/codesign");
const auto OPEN = QStringLiteral("/usr/bin/open");

// runs a tool to the end; empty if it succeeded, otherwise what went wrong
QString runTool(const QString& program, const QStringList& arguments)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    if (!process.waitForStarted())
        return UpdateInstaller::tr("Could not start %1.").arg(program);
    // copying a few hundred MB should take seconds, not minutes
    if (!process.waitForFinished(300000))
    {
        process.kill();
        process.waitForFinished();
        return UpdateInstaller::tr("%1 did not finish.").arg(program);
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        return UpdateInstaller::tr("%1 failed: %2")
            .arg(QFileInfo(program).fileName(),
                 QString::fromLocal8Bit(process.readAll()).trimmed());
    }
    return QString();
}

QString bundleIdentifier(const QString& bundle)
{
    const QSettings info(bundle + QStringLiteral("/Contents/Info.plist"), QSettings::NativeFormat);
    return info.value(QStringLiteral("CFBundleIdentifier")).toString();
}

// a disk image left mounted by an update that was interrupted
void detachStaleMount()
{
    const QString mount = UpdateInstaller::stagingDir() + QLatin1Char('/') + MOUNT_DIR;
    if (QFileInfo::exists(mount))
        runTool(HDIUTIL, { QStringLiteral("detach"), QStringLiteral("-force"), mount });
}

// Off the interface thread: mounts the .dmg, copies its app into the staging
// folder, detaches the image whatever happened, and checks the copy is
// intact and is EddyFlow. Empty if all went well, otherwise what went wrong.
QString copyAppFromDiskImage(const QString& dmg, const QString& staging,
                             const QString& runningBundle)
{
    const QString mount = staging + QLatin1Char('/') + MOUNT_DIR;
    const QString target = staging + QLatin1Char('/') + EXTRACTED_DIR;
    if (!QDir().mkpath(mount))
        return UpdateInstaller::tr("Could not create %1.").arg(mount);

    // the download is already checked against its SHA-256
    QString error = runTool(HDIUTIL, { QStringLiteral("attach"), QStringLiteral("-nobrowse"),
                                       QStringLiteral("-readonly"), QStringLiteral("-noautoopen"),
                                       QStringLiteral("-noverify"), QStringLiteral("-mountpoint"),
                                       mount, dmg });
    if (!error.isEmpty())
        return error;

    QString copy;
    const auto apps = QDir(mount).entryList({ QStringLiteral("*.app") },
                                            QDir::Dirs | QDir::NoDotAndDotDot);
    if (apps.isEmpty())
    {
        error = UpdateInstaller::tr("The disk image does not contain an app.");
    }
    else
    {
        // ditto, unlike a plain copy, keeps the frameworks' symlinks
        copy = target + QLatin1Char('/') + apps.first();
        error = runTool(DITTO, { mount + QLatin1Char('/') + apps.first(), copy });
    }
    if (!runTool(HDIUTIL, { QStringLiteral("detach"), mount }).isEmpty())
        runTool(HDIUTIL, { QStringLiteral("detach"), QStringLiteral("-force"), mount });
    QDir().rmdir(mount);
    if (!error.isEmpty())
        return error;

    error = runTool(CODESIGN, { QStringLiteral("--verify"), QStringLiteral("--deep"),
                                QStringLiteral("--strict"), copy });
    if (!error.isEmpty())
        return UpdateInstaller::tr("The app in the disk image is not intact. %1").arg(error);
    const auto id = bundleIdentifier(runningBundle);
    if (id.isEmpty() || bundleIdentifier(copy) != id)
        return UpdateInstaller::tr("The disk image does not contain %1.").arg(Defs::APP_NAME);
    return QString();
}
#endif

// everything in the staging folder but the list of files still to remove
// and, on macOS, the bundle it lists, and the folder itself once nothing is
// left in it
void removeStaged()
{
    QDir staging(UpdateInstaller::stagingDir());
    if (!staging.exists())
        return;
#if defined(Q_OS_MACOS)
    detachStaleMount();
#endif
    const auto entries = staging.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot
                                               | QDir::Hidden | QDir::System);
    for (const auto& entry : entries)
    {
        if (entry.fileName() == REPLACED_LIST || entry.fileName() == OLD_DIR)
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
[[maybe_unused]] QString folderHolding(const QString& root, const QString& fileName)
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
    bundlePath_(bundlePath()),
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
    connect(&dmgWatcher_, &QFutureWatcher<QString>::finished, this, [this]()
    {
        const auto error = dmgWatcher_.result();
        finishExtraction(error.isEmpty(), error);
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
    dmgWatcher_.waitForFinished();
}

bool UpdateInstaller::isAvailable()
{
#if defined(Q_OS_WIN)
    // a portable folder under Program Files, say, cannot be written to
    QTemporaryFile probe(programDir() + QStringLiteral("/update_probe_XXXXXX"));
    return probe.open();
#elif defined(Q_OS_MACOS)
    // the bundle is moved, so both it and its folder must be writable: not
    // so in /Applications for a standard user, on the mounted .dmg or for a
    // copy macOS runs from a translocated, read-only path
    const auto bundle = bundlePath();
    if (bundle.isEmpty() || !QFileInfo(bundle).isWritable())
        return false;
    QTemporaryFile probe(QFileInfo(bundle).absolutePath() + QStringLiteral("/update_probe_XXXXXX"));
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
#if defined(Q_OS_MACOS)
    if (archiveName.endsWith(QStringLiteral(".dmg"), Qt::CaseInsensitive))
        return QFileInfo::exists(HDIUTIL) && QFileInfo::exists(DITTO);
#endif
    return false;
}

QString UpdateInstaller::stagingDir()
{
#if defined(Q_OS_MACOS)
    const auto bundle = bundlePath();
    if (!bundle.isEmpty())
        return QFileInfo(bundle).absolutePath() + QLatin1Char('/') + Defs::UPDATE_STAGING_DIR_MAC;
#endif
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
#elif defined(Q_OS_MACOS)
    // nothing to do unless an update was installed
    if (!QFileInfo::exists(replacedList()))
        return;

    QDir staging(stagingDir());
    QStringList kept;
    for (const auto& relative : readReplacedList())
    {
        // only the bundle an update set aside, and only inside the staging folder
        if (!relative.startsWith(OLD_DIR + QLatin1Char('/')) || QDir::isAbsolutePath(relative)
            || relative.split(QLatin1Char('/')).contains(QStringLiteral("..")))
        {
            continue;
        }
        QDir old(staging.filePath(relative));
        if (old.exists() && !old.removeRecursively())
            kept << relative;
    }
    staging.rmdir(OLD_DIR);

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

#if defined(Q_OS_MACOS)
    if (archive.endsWith(QStringLiteral(".dmg"), Qt::CaseInsensitive))
    {
        const auto staging = stagingDir();
        const auto running = bundlePath_;
        dmgWatcher_.setFuture(QtConcurrent::run([archive, staging, running]()
        {
            return copyAppFromDiskImage(archive, staging, running);
        }));
        return;
    }
#endif

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
    const QString extractedDir = stagingDir() + QLatin1Char('/') + EXTRACTED_DIR;
#if defined(Q_OS_MACOS)
    // the app bundle itself is what replaces the running one
    const auto apps = QDir(extractedDir).entryInfoList({ QStringLiteral("*.app") },
                                                        QDir::Dirs | QDir::NoDotAndDotDot);
    newRoot_ = apps.isEmpty() ? QString() : apps.first().absoluteFilePath();
#else
    newRoot_ = folderHolding(extractedDir, program);
#endif
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
#elif defined(Q_OS_MACOS)
    if (newRoot_.isEmpty() || bundlePath_.isEmpty())
    {
        *error = tr("The release has not been extracted.");
        return false;
    }

    // the running bundle is set aside, not removed: it may still load
    // plugins until it closes
    const QString relative = OLD_DIR + QLatin1Char('/') + QFileInfo(bundlePath_).fileName();
    const QString aside = stagingDir() + QLatin1Char('/') + relative;
    // one an earlier update could not remove yet
    QDir(aside).removeRecursively();
    if (!QDir().mkpath(QFileInfo(aside).path()))
    {
        *error = tr("Could not create the folder for %1.").arg(aside);
        return false;
    }
    QDir dir;
    if (!dir.rename(bundlePath_, aside))
    {
        *error = tr("%1 could not be moved: it may be read-only.").arg(bundlePath_);
        return false;
    }
    if (!dir.rename(newRoot_, bundlePath_))
    {
        // undone: the old version stays where it was
        dir.rename(aside, bundlePath_);
        *error = tr("%1 could not be moved into place.").arg(bundlePath_);
        return false;
    }

    // what cleanUp() removes on the next start
    QStringList replaced = readReplacedList();
    if (!replaced.contains(relative))
        replaced << relative;
    if (!writeReplacedList(replaced))
        qWarning() << "update: could not record the replaced bundle in" << replacedList();
    newRoot_.clear();
    removeStaged();
    return true;
#else
    *error = tr("EddyFlow can update itself only on Windows and macOS.");
    return false;
#endif
}

bool UpdateInstaller::restart(const QStringList& arguments) const
{
#if defined(Q_OS_MACOS)
    // through Launch Services, as the Finder would, from the bundle's own
    // path, which now holds the new version; -n as this one is still running
    if (!bundlePath_.isEmpty())
    {
        return QProcess::startDetached(OPEN, QStringList{ QStringLiteral("-n"), bundlePath_,
                                                          QStringLiteral("--args") } + arguments);
    }
#endif
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
    // QuaZip and the copy out of a .dmg cannot be interrupted: their files
    // go once they are done
    zipWatcher_.waitForFinished();
    dmgWatcher_.waitForFinished();
    newRoot_.clear();
    removeStaged();
}
