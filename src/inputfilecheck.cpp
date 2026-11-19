/***************************************************************************
  inputfilecheck.cpp
  ------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "inputfilecheck.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include "remotesource.h"

namespace InputFileCheck
{

QString similarPattern(const QString& missingName, const QString& padding)
{
    //> The engine's timestamp: _2026-11-14T174706, followed by _adv or _exp.
    static const QRegularExpression stamp(
        QStringLiteral("\\d{4}-\\d{2}-\\d{2}T\\d{6}"));

    const QString name = QFileInfo(missingName).fileName();
    if (name.contains(stamp))
    {
        QString pattern = name;
        pattern.replace(stamp, QStringLiteral("*"));
        return pattern;
    }

    const QString suffix = QFileInfo(name).suffix();
    return QLatin1Char('*') + padding + QStringLiteral("_*")
           + (suffix.isEmpty() ? QString() : QLatin1Char('.') + suffix);
}

QStringList searchDirs(const QString& missingPath,
                       const QString& outPath,
                       const QString& projectFile)
{
    QStringList dirs;
    auto add = [&dirs](const QString& dir)
    {
        if (dir.isEmpty()) { return; }
        const QFileInfo info(dir);
        if (!info.isDir()) { return; }
        const QString clean = QDir::cleanPath(info.absoluteFilePath());
        for (const auto& d : dirs)
        {
            if (samePath(d, clean)) { return; }
        }
        dirs << clean;
    };

    const QFileInfo missing(missingPath);
    const QString ownDir = missing.absolutePath();
    add(ownDir);
    add(outPath);

    //> Beside it, one level only: a share holding years of runs is listed
    //> once here, not walked.
    const QDir parent(QFileInfo(ownDir).absolutePath());
    if (parent.exists() && !samePath(parent.absolutePath(), ownDir))
    {
        const auto siblings = parent.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto& sibling : siblings)
        {
            add(sibling.absoluteFilePath());
        }
    }

    if (!projectFile.isEmpty())
    {
        add(QFileInfo(projectFile).absolutePath());
    }
    return dirs;
}

QString findSimilar(const QString& pattern,
                    const QStringList& dirs,
                    const QString& exclude)
{
    QFileInfo best;
    for (const auto& dir : dirs)
    {
        const auto files = QDir(dir).entryInfoList(
            QStringList{pattern}, QDir::Files | QDir::Readable, QDir::Time);
        for (const auto& file : files)
        {
            if (!exclude.isEmpty() && file.fileName().contains(exclude)) { continue; }
            if (!best.exists() || file.lastModified() > best.lastModified())
            {
                best = file;
            }
        }
    }
    return best.exists() ? QDir::toNativeSeparators(best.absoluteFilePath())
                         : QString();
}

bool usable(const QString& path)
{
    if (path.trimmed().isEmpty()) { return false; }
    if (RemoteSource::isRemote(path)) { return true; }
    return QFileInfo(path).isFile();
}

bool samePath(const QString& a, const QString& b)
{
    QString ca = QDir::cleanPath(QDir::fromNativeSeparators(a.trimmed()));
    QString cb = QDir::cleanPath(QDir::fromNativeSeparators(b.trimmed()));
    //> The browse field shows a file's canonical path, which can be spelled
    //> differently from the one the project saved and still be that file.
    //> Two files that exist are compared as the files they are.
    const QFileInfo fa(ca);
    const QFileInfo fb(cb);
    if (fa.exists() && fb.exists())
    {
        ca = fa.canonicalFilePath();
        cb = fb.canonicalFilePath();
    }
#ifdef Q_OS_WIN
    return ca.compare(cb, Qt::CaseInsensitive) == 0;
#else
    return ca == cb;
#endif
}

}  // namespace InputFileCheck
