/***************************************************************************
  remotesource.h
  --------------
  Copyright © 2026, ETH Zurich, Jonathan Muller

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

#ifndef REMOTESOURCE_H
#define REMOTESOURCE_H

#include <QList>
#include <QString>
#include <QStringList>

class QWidget;

///
/// \brief Inputs from a shared Google Drive or Dropbox link.
///
/// Any input setting - raw data folder, metadata, biomet, planar fit, time
/// lag and spectral assessment files, cospectra folders - may hold a link to
/// a folder or file shared with "anyone with the link". The engine downloads
/// what it needs itself (src_common/remote_source.f90 in eddyflow-engine);
/// the interface only needs to browse the share, list it, and fetch the odd
/// file it reads for itself, such as a metadata file to edit or a GHG
/// archive to take the embedded metadata from.
///
/// A value the interface writes is the item's own link plus a fragment,
///     <link>#root=<the drive's link, encoded>&path=<path in the drive, encoded>
/// so the field can say where it points, and the browser can reopen there.
/// The engine ignores the fragment but for the file name at the end of path.
///
/// How each provider is listed, as found by probing it on 2026-09-25, is in
/// the engine's remote_source.f90; the two must agree.
///
/// Everything here blocks, running an event loop while it waits: the calls
/// come from dialogs the user is waiting on anyway.
///
class RemoteSource
{
public:
    struct Entry
    {
        QString name;
        bool isDir = false;
        QString link;       // the item's own link, without fragment
        qint64 size = -1;   // -1 when the provider does not say (Google)
        QString modified;   // as the provider shows it; empty when unknown
    };

    enum class Provider { None, GoogleDrive, Dropbox };

    // --- links ---------------------------------------------------------
    static bool isRemote(const QString& path);
    static Provider provider(const QString& link);
    /// QDir::cleanPath for a local path; a link unchanged, since cleanPath
    /// would turn its https:// into https:/
    static QString cleanPath(const QString& path);
    /// The link without fragment and without the noise a share link carries
    /// (usp=, st=, dl=)
    static QString canonical(const QString& link);
    /// "Google Drive: site/2019-03" - for display only
    static QString displayName(const QString& link);
    static QString withLocation(const QString& link, const QString& rootLink,
                                const QString& relPath);
    static QString rootOf(const QString& link);
    static QString relPathOf(const QString& link);
    /// Whether the link can only be a file (a Drive /file/d/ link)
    static bool isFileLink(const QString& link);

    // --- network -------------------------------------------------------
    /// Entries of a shared folder; false, and error set, if it cannot be read
    static bool listFolder(const QString& folderLink, QList<Entry>* entries,
                           QString* error);
    /// Download a linked file to dest; false, and error set, on failure
    static bool download(const QString& fileLink, const QString& dest,
                         QString* error);

    /// FileUtils::getFiles for a link: the files matching glob, in
    /// subfolders too if recurse, as the paths they will have in the local
    /// cache. Nothing is downloaded until ensureLocal is asked for one.
    static QStringList listFiles(const QString& folderLink, const QString& glob,
                                 bool recurse, QWidget* parent = nullptr);
    /// A local file for pathOrLink: a path listFiles returned, downloaded
    /// now if it is not yet; a link to a file, downloaded into the cache; or
    /// a local path, unchanged. Empty, after a warning window, on failure.
    static QString ensureLocal(const QString& pathOrLink, QWidget* parent = nullptr);

    /// The warning window every failure here ends in
    static void warn(QWidget* parent, const QString& what, const QString& detail);

    // --- the drive the open project uses --------------------------------
    static QString currentDrive();
    static void setCurrentDrive(const QString& rootLink);
    static void clearCurrentDrive();
    /// Take the drive from the first of these values that is a link
    static void adoptDriveFrom(const QStringList& values);
    static QStringList recentDrives();

private:
    static QString cacheDir();
};

#endif // REMOTESOURCE_H
