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
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QGridLayout>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include "defs.h"
#include "downloadmanager.h"
#include "stringutils.h"
#include "widget_utils.h"

UpdateDialog::UpdateDialog(QWidget *parent) :
    QDialog(parent),
    updateManager(nullptr),
    isNewVersionAvailable_(false),
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

    okButton = WidgetUtils::createCommonButton(this, tr("Ok"));

    yesButton = new QPushButton(tr("Yes"));
    yesButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    yesButton->setDefault(true);
    yesButton->setProperty("commonButton", true);

    noButton = new QPushButton(tr("No"));
    noButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    noButton->setDefault(true);
    noButton->setProperty("commonButton", true);

    auto dialogLayout = new QGridLayout(this);
    dialogLayout->addWidget(groupTitle, 0, 0, 1, -1);
    dialogLayout->addWidget(hrLabel, 1, 0, 1, -1);
    dialogLayout->addWidget(msgLabel, 2, 0, 1, -1);
    dialogLayout->addWidget(okButton, 3, 0, 1, 2, Qt::AlignCenter);
    dialogLayout->addWidget(yesButton, 3, 0, 1, 1, Qt::AlignCenter);
    dialogLayout->addWidget(noButton, 3, 1, 1, 1, Qt::AlignCenter);
    dialogLayout->setVerticalSpacing(10);
    dialogLayout->setContentsMargins(30, 30, 15, 30);
    dialogLayout->setSizeConstraint(QLayout::SetFixedSize);
    setLayout(dialogLayout);

    connect(okButton, &QPushButton::clicked,
            this, &UpdateDialog::close);
    connect(yesButton, &QPushButton::clicked,
            this, &UpdateDialog::showDownloadPage);
    connect(noButton, &QPushButton::clicked,
            this, &UpdateDialog::close);

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

void UpdateDialog::initialize()
{
    msgLabel->clear();
    msgLabel->setText(tr("<b>Retrieving information...</b>"));
    okButton->setVisible(true);
    yesButton->setVisible(false);
    noButton->setVisible(false);
}

void UpdateDialog::close()
{
    if (updateManager)
    {
        updateManager->abort();
    }

    if (isVisible())
    {
        hide();
    }

    downloadTimer_->stop();
}

void UpdateDialog::getNewVersion(const QString& version)
{
    msgLabel->setText(tr("<p><b>A newer version of %1 (version %2) is available.<br />"
                         "Do you want to open the releases page to download it?</b></p>"
                         "<p>The new version does not overwrite previously installed versions.</p>"
                         ).arg(Defs::APP_NAME, version));
    msgLabel->setOpenExternalLinks(true);
    okButton->setVisible(false);
    yesButton->setVisible(true);
    noButton->setVisible(true);
}

void UpdateDialog::noNewVersion()
{
    msgLabel->setText(tr("<b>No newer version of %1 is available at this time.</b>").arg(Defs::APP_NAME));
    okButton->setVisible(true);
    yesButton->setVisible(false);
    noButton->setVisible(false);
}

void UpdateDialog::noConnection()
{
    msgLabel->setText(tr("<b>No connection available or connection error.</b>"));
    okButton->setVisible(true);
    yesButton->setVisible(false);
    noButton->setVisible(false);
}

void UpdateDialog::downloadError()
{
    msgLabel->setText(tr("<b>Download error.</b>"));
    okButton->setVisible(true);
    yesButton->setVisible(false);
    noButton->setVisible(false);
}

void UpdateDialog::checkUpdate()
{
    if (!updateManager)
    {
        updateManager = new DownloadManager(this);

        connect(updateManager, &DownloadManager::downloadComplete,
                this, &UpdateDialog::useDownloadResults);
    }
    QTimer::singleShot(0, updateManager, &DownloadManager::execute);

    downloadTimer_->start();
}

bool UpdateDialog::hasNewVersion()
{
    return isNewVersionAvailable_;
}

void UpdateDialog::showDownloadPage()
{
    QDesktopServices::openUrl(QUrl(Defs::EP_LATEST_RELEASE));
    close();
}

void UpdateDialog::downloadTimeout()
{
    noConnection();
}

void UpdateDialog::useDownloadResults()
{
    QByteArray data = updateManager->getVersionNr();

    QString newVersion;
    QUrl resolvedUrl = updateManager->getResolvedUrl();
    QRegularExpression tagExpression(QStringLiteral("/releases/tag/([^/?#]+)"));
    QRegularExpressionMatch tagMatch = tagExpression.match(resolvedUrl.path());
    if (tagMatch.hasMatch())
    {
        QString tag = tagMatch.captured(1);
        if (tag.startsWith(QLatin1Char('v')))
            tag.remove(0, 1);
        newVersion = tag.trimmed();
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (newVersion.isEmpty()
        && parseError.error == QJsonParseError::NoError
        && doc.isObject())
    {
        QString tag = doc.object().value(QStringLiteral("tag_name")).toString();
        if (tag.startsWith(QLatin1Char('v')))
            tag.remove(0, 1);
        newVersion = tag.trimmed();
    }

    if (!newVersion.isEmpty())
    {
        if (StringUtils::isNewVersion(newVersion, Defs::APP_VERSION_STR))
        {
            isNewVersionAvailable_ = true;
            getNewVersion(newVersion);
        }
        else
        {
            isNewVersionAvailable_ = false;
            noNewVersion();
        }
    }
    else
    {
        isNewVersionAvailable_ = false;
        downloadError();
    }
    downloadTimer_->stop();
}
