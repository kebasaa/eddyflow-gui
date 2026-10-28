/***************************************************************************
  updatedialog.cpp
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

#include "updatedialog.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QGridLayout>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QUrl>

#include <algorithm>

#include "defs.h"
#include "downloadmanager.h"
#include "stringutils.h"
#include "updateinstaller.h"
#include "widget_utils.h"

UpdateDialog::UpdateDialog(QWidget *parent) :
    QDialog(parent),
    updateManager(new DownloadManager(this)),
    installer_(new UpdateInstaller(this)),
    isNewVersionAvailable_(false),
    stage_(Stage::Idle),
    downloadTimer_(nullptr)
{
    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("Check for Updates"));
    WidgetUtils::removeContextHelpButton(this);

    auto groupTitle = new QLabel;
    groupTitle->setText(tr("Checking for newer versions."));

    auto hrLabel = new QLabel;
    hrLabel->setObjectName(QStringLiteral("hrLabel"));
    hrLabel->setMinimumWidth(400);

    msgLabel = new QLabel(tr("Retrieving information..."));
    msgLabel->setStyleSheet(QStringLiteral("QLabel {margin-bottom: 15px;}"));
    msgLabel->setWordWrap(true);

    progressBar_ = new QProgressBar;
    progressBar_->setMinimumWidth(400);
    progressBar_->setVisible(false);

    okButton = WidgetUtils::createCommonButton(this, tr("Ok"));
    downloadButton_ = WidgetUtils::createCommonButton(this, tr("Download and install"));
    pageButton_ = WidgetUtils::createCommonButton(this, tr("Go to download page"));
    laterButton_ = WidgetUtils::createCommonButton(this, tr("Later"));
    cancelButton_ = WidgetUtils::createCommonButton(this, tr("Cancel"));
    installButton_ = WidgetUtils::createCommonButton(this, tr("Install and restart"));

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    for (auto button : { downloadButton_, installButton_, pageButton_, okButton,
                         laterButton_, cancelButton_ })
    {
        buttonLayout->addWidget(button);
    }
    buttonLayout->addStretch();

    auto dialogLayout = new QGridLayout(this);
    dialogLayout->addWidget(groupTitle, 0, 0, 1, -1);
    dialogLayout->addWidget(hrLabel, 1, 0, 1, -1);
    dialogLayout->addWidget(msgLabel, 2, 0, 1, -1);
    dialogLayout->addWidget(progressBar_, 3, 0, 1, -1);
    dialogLayout->addLayout(buttonLayout, 4, 0, 1, -1);
    dialogLayout->setVerticalSpacing(10);
    dialogLayout->setContentsMargins(30, 30, 15, 30);
    dialogLayout->setSizeConstraint(QLayout::SetFixedSize);
    setLayout(dialogLayout);

    connect(okButton, &QPushButton::clicked,
            this, &UpdateDialog::close);
    connect(laterButton_, &QPushButton::clicked,
            this, &UpdateDialog::close);
    connect(cancelButton_, &QPushButton::clicked,
            this, &UpdateDialog::close);
    connect(pageButton_, &QPushButton::clicked,
            this, &UpdateDialog::showDownloadPage);
    connect(downloadButton_, &QPushButton::clicked,
            this, &UpdateDialog::startDownload);
    connect(installButton_, &QPushButton::clicked,
            this, &UpdateDialog::installRequested);

    connect(updateManager, &DownloadManager::downloadComplete,
            this, &UpdateDialog::useDownloadResults);
    connect(updateManager, &DownloadManager::progress,
            this, &UpdateDialog::showDownloadProgress);
    connect(updateManager, &DownloadManager::fileFinished,
            this, &UpdateDialog::downloadFinished);
    connect(installer_, &UpdateInstaller::extracted,
            this, &UpdateDialog::extractionFinished);

    QTimer::singleShot(0, this, &UpdateDialog::initialize);

    downloadTimer_ = new QTimer(this);
    downloadTimer_->setInterval(10000);
    downloadTimer_->setSingleShot(true);
    connect(downloadTimer_, &QTimer::timeout,
            this, &UpdateDialog::downloadTimeout);
}

UpdateDialog::~UpdateDialog()
{
}

void UpdateDialog::showButtons(const QList<QPushButton*>& shown)
{
    for (auto button : { downloadButton_, installButton_, pageButton_, okButton,
                         laterButton_, cancelButton_ })
    {
        button->setVisible(shown.contains(button));
        button->setDefault(!shown.isEmpty() && button == shown.first());
    }
}

void UpdateDialog::initialize()
{
    // a download under way, or a release waiting to be installed, stays
    if (isBusy())
        return;

    msgLabel->clear();
    msgLabel->setText(tr("<b>Retrieving information...</b>"));
    progressBar_->setVisible(false);
    showButtons({ okButton });
}

bool UpdateDialog::isBusy() const
{
    return stage_ != Stage::Idle;
}

void UpdateDialog::close()
{
    // Later, Cancel and closing the window all leave EddyFlow as it was
    cancelUpdate();
    updateManager->abort();

    if (isVisible())
    {
        hide();
    }

    downloadTimer_->stop();
}

void UpdateDialog::reject()
{
    close();
}

void UpdateDialog::cancelUpdate()
{
    if (stage_ == Stage::Idle)
        return;
    stage_ = Stage::Idle;
    updateManager->abortFile();
    installer_->discard();
    progressBar_->setVisible(false);
}

bool UpdateDialog::canInstall() const
{
    return UpdateInstaller::isAvailable()
           && updateManager->assetUrl().isValid()
           && !updateManager->assetUrl().isEmpty()
           && UpdateInstaller::canExtract(updateManager->assetName());
}

void UpdateDialog::getNewVersion(const QString& version)
{
    QString text = tr("<p><b>%1 %2 is available</b> (you have %3).</p>")
                       .arg(Defs::APP_NAME, version, Defs::APP_VERSION_STR);
    const auto name = updateManager->releaseName();
    if (!name.isEmpty())
        text += QStringLiteral("<p><i>%1</i></p>").arg(name.toHtmlEscaped());

    if (canInstall())
    {
        text += tr("<p>Do you want to download and install it? Nothing is installed "
                   "until the download is ready and you agree to restart %1. Your "
                   "projects and settings are kept.</p>").arg(Defs::APP_NAME);
        pageButton_->setText(tr("Open release page"));
        showButtons({ downloadButton_, pageButton_, laterButton_ });
    }
    else
    {
        text += tr("<p>Do you want to open the download page in your web browser?</p>");
        pageButton_->setText(tr("Go to download page"));
        showButtons({ pageButton_, laterButton_ });
    }
    msgLabel->setText(text);
    progressBar_->setVisible(false);
}

void UpdateDialog::noNewVersion()
{
    msgLabel->setText(tr("<b>No newer version of %1 is available at this time.</b>").arg(Defs::APP_NAME));
    showButtons({ okButton });
}

void UpdateDialog::noConnection()
{
    msgLabel->setText(tr("<b>No connection available or connection error.</b>"));
    showButtons({ okButton });
}

void UpdateDialog::downloadError()
{
    msgLabel->setText(tr("<b>Download error.</b>"));
    showButtons({ okButton });
}

void UpdateDialog::updateError(const QString& error)
{
    stage_ = Stage::Idle;
    updateManager->abortFile();
    installer_->discard();
    progressBar_->setVisible(false);
    msgLabel->setText(tr("<p><b>%1 %2 could not be installed.</b> %1 %3 is unchanged.</p><p>%4</p>")
                          .arg(Defs::APP_NAME, newVersion_, Defs::APP_VERSION_STR,
                               error.toHtmlEscaped()));
    pageButton_->setText(tr("Open release page"));
    showButtons({ pageButton_, okButton });
    show();
    raise();
    activateWindow();
}

void UpdateDialog::checkUpdate()
{
    if (isBusy())
        return;
    QTimer::singleShot(0, updateManager, &DownloadManager::execute);

    downloadTimer_->start();
}

void UpdateDialog::showDownloadPage()
{
    // the latest release, whichever it is: GitHub redirects there
    QDesktopServices::openUrl(QUrl(Defs::EP_LATEST_RELEASE));
    close();
}

void UpdateDialog::downloadTimeout()
{
    updateManager->abort();
    isNewVersionAvailable_ = false;
    noConnection();
    emit checkFinished(false);
}

void UpdateDialog::useDownloadResults()
{
    downloadTimer_->stop();

    newVersion_ = updateManager->releaseVersion();
    if (newVersion_.isEmpty())
    {
        isNewVersionAvailable_ = false;
        downloadError();
    }
    else if (StringUtils::isNewVersion(newVersion_, Defs::APP_VERSION_STR))
    {
        isNewVersionAvailable_ = true;
        getNewVersion(newVersion_);
    }
    else
    {
        isNewVersionAvailable_ = false;
        noNewVersion();
    }
    emit checkFinished(isNewVersionAvailable_);
}

void UpdateDialog::startDownload()
{
    archivePath_ = installer_->prepare(updateManager->assetName());
    if (archivePath_.isEmpty())
    {
        updateError(tr("The folder %1 could not be created.")
                        .arg(QDir::toNativeSeparators(UpdateInstaller::stagingDir())));
        return;
    }

    stage_ = Stage::Downloading;
    msgLabel->setText(tr("<b>Downloading %1 %2...</b>").arg(Defs::APP_NAME, newVersion_));
    progressBar_->setRange(0, 0);
    progressBar_->setTextVisible(true);
    progressBar_->setFormat(QString());
    progressBar_->setVisible(true);
    showButtons({ cancelButton_ });

    updateManager->downloadFile(updateManager->assetUrl(), archivePath_);
}

void UpdateDialog::showDownloadProgress(qint64 received, qint64 total)
{
    if (stage_ != Stage::Downloading)
        return;
    if (total <= 0)
        total = updateManager->assetSize();
    if (total <= 0)
        return;
    progressBar_->setRange(0, 1000);
    progressBar_->setValue(static_cast<int>(1000 * std::min(received, total) / total));
    progressBar_->setFormat(tr("%1 of %2 MB")
                                .arg(received / 1e6, 0, 'f', 1)
                                .arg(total / 1e6, 0, 'f', 1));
}

void UpdateDialog::downloadFinished(bool ok, const QString& error)
{
    if (stage_ != Stage::Downloading)
        return;
    if (!ok)
    {
        updateError(tr("The download failed: %1").arg(error));
        return;
    }

    QString why;
    if (!UpdateInstaller::verify(archivePath_, updateManager->assetSize(),
                                 updateManager->assetSha256(), &why))
    {
        updateError(why);
        return;
    }

    stage_ = Stage::Extracting;
    msgLabel->setText(tr("<b>Extracting %1 %2...</b>").arg(Defs::APP_NAME, newVersion_));
    progressBar_->setRange(0, 0);
    progressBar_->setFormat(QString());
    showButtons({ cancelButton_ });
    installer_->extract(archivePath_);
}

void UpdateDialog::extractionFinished(bool ok, const QString& error)
{
    if (stage_ != Stage::Extracting)
        return;
    if (!ok)
    {
        updateError(error);
        return;
    }

    // the second question: nothing has changed yet
    stage_ = Stage::Ready;
    progressBar_->setVisible(false);
    msgLabel->setText(tr("<p><b>%1 %2 is ready to install.</b></p>"
                         "<p>%1 will close and restart. If the current project has "
                         "unsaved changes, you will be asked to save them first.</p>")
                          .arg(Defs::APP_NAME, newVersion_));
    showButtons({ installButton_, cancelButton_ });
    show();
    raise();
    activateWindow();
}

bool UpdateDialog::installAndRestart(const QStringList& arguments)
{
    if (stage_ != Stage::Ready)
        return false;

    QString error;
    if (!installer_->install(&error))
    {
        updateError(error);
        return false;
    }
    stage_ = Stage::Idle;
    hide();

    if (!installer_->restart(arguments))
    {
        QMessageBox::information(parentWidget(), tr("Update installed"),
                                 tr("%1 %2 has been installed, but could not be started. "
                                    "Please start %1 again.").arg(Defs::APP_NAME, newVersion_));
    }
    return true;
}
