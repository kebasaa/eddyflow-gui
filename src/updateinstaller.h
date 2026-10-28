/***************************************************************************
  updateinstaller.h
  -----------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef UPDATEINSTALLER_H
#define UPDATEINSTALLER_H

#include <QFutureWatcher>
#include <QObject>
#include <QStringList>

class QProcess;

/// Puts a downloaded release in place of the running one, on Windows.
///
/// Everything happens beside the program, in its .update folder: the archive
/// is downloaded there and extracted there, so moving the new files into
/// place is a rename on the same drive. Windows will not overwrite a running
/// program or a loaded library, but it will rename one, so each file being
/// replaced is first set aside as <name>.old; if anything fails, every step
/// is undone and the old version carries on. The .old files are listed in
/// .update/replaced.txt and removed by cleanUp() on the next start - only
/// those, never anything else that happens to end in .old.
///
/// A release is a .zip, opened with QuaZip, or a .7z, opened with the 7-Zip
/// the portable folder ships in bin. On Linux and macOS, where EddyFlow comes
/// as a package, isAvailable() is false and nothing here touches the disk.
class UpdateInstaller : public QObject
{
    Q_OBJECT

public:
    explicit UpdateInstaller(QObject* parent = nullptr);
    ~UpdateInstaller();

    /// Whether this copy of EddyFlow can update itself: Windows, and a
    /// program folder it may write to.
    static bool isAvailable();
    /// Whether an archive of this name can be opened here.
    static bool canExtract(const QString& archiveName);
    /// The folder next to the program that downloads and extractions go to.
    static QString stagingDir();
    /// Removes what the last update left behind: the files it set aside,
    /// once nothing holds them any more, and the staging folder.
    static void cleanUp();

    /// Where to download an archive of this name; an earlier download and
    /// extraction are cleared away first. Empty if the folder cannot be made.
    QString prepare(const QString& archiveName);
    /// Checks a download against the size and SHA-256 the release gave.
    static bool verify(const QString& archive, qint64 size, const QByteArray& sha256,
                       QString* error);
    /// Extracts the archive into the staging folder, then emits extracted().
    void extract(const QString& archive);
    /// Moves the extracted files over the program's own. On failure every
    /// file is put back as it was and error says why.
    bool install(QString* error);
    /// Starts the new program from the program folder.
    bool restart(const QStringList& arguments) const;
    /// Stops an extraction and removes the download and what was extracted.
    void discard();

signals:
    void extracted(bool ok, const QString& error);

private:
    void finishExtraction(bool ok, const QString& error);

    QString programPath_;
    QString newRoot_;
    QProcess* process_;
    QFutureWatcher<QStringList> zipWatcher_;
    bool extracting_;
};

#endif // UPDATEINSTALLER_H
