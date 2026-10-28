/***************************************************************************
  downloadmanager.h
  -------------------
  Copyright © 2016-2018, LI-COR Biosciences, Antonio Forgione
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
 ***************************************************************************/

#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QUrl>

class QSaveFile;

/// Asks GitHub for the latest release, and downloads its portable archive.
///
/// The release comes from the GitHub API rather than the release page, as
/// only the API lists the files a release carries: the version is its tag,
/// and the archive the first asset named *portable* that is a .zip or a .7z,
/// a zip preferred. GitHub publishes a SHA-256 for every asset, which the
/// download is checked against.
class DownloadManager : public QObject
{
    Q_OBJECT

public:
    explicit DownloadManager(QObject* parent = nullptr);
    ~DownloadManager();

    void abort();

    /// The latest release, once downloadComplete() has been emitted; the
    /// version is empty if the request failed.
    QString releaseVersion() const;
    QString releaseName() const;
    /// Its portable archive; the URL is empty if it has none.
    QUrl assetUrl() const;
    QString assetName() const;
    qint64 assetSize() const;
    /// The raw digest, empty if GitHub gave none.
    QByteArray assetSha256() const;

    /// Downloads url into path, emitting progress() and then fileFinished().
    void downloadFile(const QUrl& url, const QString& path);
    /// Stops a download; nothing is left at its path.
    void abortFile();

signals:
    void downloadComplete();
    void progress(qint64 received, qint64 total);
    void fileFinished(bool ok, const QString& error);

public slots:
    void execute();

private slots:
    void downloadError(QNetworkReply::NetworkError error);
    void downloadFinished();
    void fileDataReady();
    void fileDownloadFinished();

private:
    QNetworkRequest request(const QUrl& url) const;
    void parseRelease(const QByteArray& data);

    QNetworkAccessManager manager;
    QPointer<QNetworkReply> reply;
    QPointer<QNetworkReply> fileReply_;
    QSaveFile* file_;

    QString version_;
    QString name_;
    QUrl assetUrl_;
    QString assetName_;
    qint64 assetSize_;
    QByteArray assetSha256_;
};

#endif  // DOWNLOADMANAGER_H
