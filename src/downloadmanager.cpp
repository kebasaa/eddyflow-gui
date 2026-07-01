/***************************************************************************
  downloadmanager.cpp
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

#include "downloadmanager.h"

#include <QDebug>
#include <QNetworkRequest>
#include <QUrl>

#include <QThread>

#include "defs.h"

DownloadManager::DownloadManager(QObject *parent) :
    QObject(parent),
    reply(nullptr),
    versionNr(QByteArray())
{
}

DownloadManager::~DownloadManager()
{
}

void DownloadManager::abort()
{
    if (reply)
    {
        if (reply->isRunning())
        {
            disconnect(reply, &QNetworkReply::finished,
                       this, &DownloadManager::downloadFinished);
            reply->abort();
        }
        reply->deleteLater();
    }
}

void DownloadManager::execute()
{
    QUrl url = QUrl::fromEncoded(Defs::EP_LATEST_RELEASE.toLocal8Bit());
    get(url);
}

void DownloadManager::get(const QUrl &url)
{
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent", Defs::EP_USER_AGENT.toLatin1());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    reply = manager.get(request);

    connect(reply, &QNetworkReply::downloadProgress,
            this, &DownloadManager::downloadProgress);
    connect(reply, &QNetworkReply::finished,
            this, &DownloadManager::downloadFinished);
    connect(reply, &QNetworkReply::errorOccurred,
            this, &DownloadManager::downloadError);
}

void DownloadManager::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    qDebug() << "Downloaded " << bytesReceived << "of " << bytesTotal;
}

void DownloadManager::downloadError(QNetworkReply::NetworkError error)
{
    qDebug() << "Reply error" << error;
    qDebug() << reply->errorString();
    versionNr = QByteArray();
}

void DownloadManager::downloadFinished()
{
    if (reply)
    {
        resolvedUrl = reply->url();
        if (reply->error())
        {
            qDebug() << "Request Failed: error" << reply->error();
        }
        else
        {
            qDebug() << "Request Succeded";
        }

        if (!reply->error())
        {
            versionNr = reply->readAll();
        }

        reply->deleteLater();
        emit downloadComplete();
    }
}

QByteArray DownloadManager::getVersionNr() const
{
    return versionNr;
}

QUrl DownloadManager::getResolvedUrl() const
{
    return resolvedUrl;
}
