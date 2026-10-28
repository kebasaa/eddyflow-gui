/***************************************************************************
  updatedialog.h
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

#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>
#include <QStringList>

////////////////////////////////////////////////////////////////////////////////
/// \file src/updatedialog.h
/// \brief
/// \version
/// \date
/// \author Antonio Forgione
/// \note
/// \sa
/// \bug
/// \deprecated
/// \test
/// \todo
////////////////////////////////////////////////////////////////////////////////

class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

class DownloadManager;
class UpdateInstaller;

/// Tells of a newer release and, on Windows, puts it in place.
///
/// The check reads the latest release from GitHub. On Windows, with a
/// portable archive in the release and a program folder EddyFlow may write
/// to, the user can download and install it: nothing is downloaded until
/// they ask, and nothing is installed until they agree a second time, once
/// the download is ready. Installing is the main window's call, as it first
/// has to be allowed to close (a computation stopped, a project saved): it
/// answers installRequested() with installAndRestart(). Everywhere else the
/// dialog only offers the download page.
class UpdateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UpdateDialog(QWidget* parent = nullptr);
    ~UpdateDialog();

    void checkUpdate();
    /// Whether a download, an extraction or a release waiting to be
    /// installed is under way, which a new check must not throw away.
    bool isBusy() const;
    /// Puts the downloaded release in place and starts it; false, with the
    /// reason shown and the current version untouched, if it fails.
    bool installAndRestart(const QStringList& arguments);

signals:
    void checkFinished(bool newer);
    void installRequested();

public slots:
    void close();
    void initialize();
    void reject() override;

private slots:
    void showDownloadPage();
    void downloadTimeout();

    void useDownloadResults();
    void startDownload();
    void showDownloadProgress(qint64 received, qint64 total);
    void downloadFinished(bool ok, const QString& error);
    void extractionFinished(bool ok, const QString& error);

private:
    enum class Stage { Idle, Downloading, Extracting, Ready };

    QPushButton *okButton;
    QPushButton *downloadButton_;
    QPushButton *pageButton_;
    QPushButton *laterButton_;
    QPushButton *cancelButton_;
    QPushButton *installButton_;
    QLabel *msgLabel;
    QProgressBar *progressBar_;

    DownloadManager* updateManager;
    UpdateInstaller* installer_;

    void showButtons(const QList<QPushButton*>& shown);
    bool canInstall() const;
    void getNewVersion(const QString& version);
    void noNewVersion();
    void noConnection();
    void downloadError();
    void updateError(const QString& error);
    void cancelUpdate();

    bool isNewVersionAvailable_;
    QString newVersion_;
    QString archivePath_;
    Stage stage_;

    QTimer* downloadTimer_;
};

#endif // UPDATEDIALOG_H
