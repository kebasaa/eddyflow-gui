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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>

#include "defs.h"

DownloadManager::DownloadManager(QObject *parent) :
    QObject(parent),
    reply(nullptr),
    fileReply_(nullptr),
    file_(nullptr),
    assetSize_(0)
{
}

DownloadManager::~DownloadManager()
{
    abort();
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
    abortFile();
}

void DownloadManager::execute()
{
    // a check already under way gives way to this one
    if (reply)
    {
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    version_.clear();

    auto latest = request(QUrl(Defs::EP_LATEST_RELEASE_API));
    latest.setRawHeader("Accept", "application/vnd.github+json");
    reply = manager.get(latest);

    connect(reply, &QNetworkReply::finished,
            this, &DownloadManager::downloadFinished);
    connect(reply, &QNetworkReply::errorOccurred,
            this, &DownloadManager::downloadError);
}

QNetworkRequest DownloadManager::request(const QUrl &url) const
{
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent", Defs::EP_USER_AGENT.toLatin1());
    // the archive is served from GitHub's asset host, by redirect
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

void DownloadManager::downloadError(QNetworkReply::NetworkError error)
{
    qDebug() << "Reply error" << error;
    if (reply)
        qDebug() << reply->errorString();
}

void DownloadManager::downloadFinished()
{
    if (reply)
    {
        if (reply->error())
        {
            qDebug() << "Request Failed: error" << reply->error();
        }
        else
        {
            parseRelease(reply->readAll());
        }

        reply->deleteLater();
        emit downloadComplete();
    }
}

void DownloadManager::parseRelease(const QByteArray &data)
{
    version_.clear();
    name_.clear();
    assetUrl_.clear();
    assetName_.clear();
    assetSize_ = 0;
    assetSha256_.clear();

    const auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return;
    const auto release = doc.object();

    auto tag = release.value(QStringLiteral("tag_name")).toString().trimmed();
    if (tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        tag.remove(0, 1);
    version_ = tag;
    name_ = release.value(QStringLiteral("name")).toString().trimmed();

    // the portable archive: a zip if there is one, as it needs no helper
    QJsonObject chosen;
    const auto assets = release.value(QStringLiteral("assets")).toArray();
    for (const auto& value : assets)
    {
        const auto asset = value.toObject();
        const auto name = asset.value(QStringLiteral("name")).toString();
        if (!name.contains(QStringLiteral("portable"), Qt::CaseInsensitive))
            continue;
        if (name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive))
        {
            chosen = asset;
            break;
        }
        if (name.endsWith(QStringLiteral(".7z"), Qt::CaseInsensitive) && chosen.isEmpty())
            chosen = asset;
    }
    if (chosen.isEmpty())
        return;

    assetName_ = chosen.value(QStringLiteral("name")).toString();
    assetUrl_ = QUrl(chosen.value(QStringLiteral("browser_download_url")).toString());
    assetSize_ = chosen.value(QStringLiteral("size")).toInteger();
    // "sha256:<64 hex digits>"
    const auto digest = chosen.value(QStringLiteral("digest")).toString();
    if (digest.startsWith(QStringLiteral("sha256:")))
    {
        const auto sha256 = QByteArray::fromHex(digest.mid(7).toLatin1());
        if (sha256.size() == 32)
            assetSha256_ = sha256;
    }
}

QString DownloadManager::releaseVersion() const
{
    return version_;
}

QString DownloadManager::releaseName() const
{
    return name_;
}

QUrl DownloadManager::assetUrl() const
{
    return assetUrl_;
}

QString DownloadManager::assetName() const
{
    return assetName_;
}

qint64 DownloadManager::assetSize() const
{
    return assetSize_;
}

QByteArray DownloadManager::assetSha256() const
{
    return assetSha256_;
}

void DownloadManager::downloadFile(const QUrl &url, const QString &path)
{
    abortFile();

    // written as it arrives, and only there once complete
    file_ = new QSaveFile(path, this);
    if (!file_->open(QIODevice::WriteOnly))
    {
        const auto error = file_->errorString();
        delete file_;
        file_ = nullptr;
        emit fileFinished(false, error);
        return;
    }

    fileReply_ = manager.get(request(url));
    connect(fileReply_, &QNetworkReply::readyRead,
            this, &DownloadManager::fileDataReady);
    connect(fileReply_, &QNetworkReply::downloadProgress,
            this, &DownloadManager::progress);
    connect(fileReply_, &QNetworkReply::finished,
            this, &DownloadManager::fileDownloadFinished);
}

void DownloadManager::abortFile()
{
    if (fileReply_)
    {
        disconnect(fileReply_, nullptr, this, nullptr);
        fileReply_->abort();
        fileReply_->deleteLater();
        fileReply_ = nullptr;
    }
    if (file_)
    {
        file_->cancelWriting();
        delete file_;
        file_ = nullptr;
    }
}

void DownloadManager::fileDataReady()
{
    if (!fileReply_ || !file_)
        return;
    if (file_->write(fileReply_->readAll()) < 0)
    {
        const auto error = file_->errorString();
        abortFile();
        emit fileFinished(false, error);
    }
}

void DownloadManager::fileDownloadFinished()
{
    if (!fileReply_ || !file_)
        return;

    fileDataReady();
    if (!file_)
        return;   // a write failed, and fileFinished() has been emitted

    const bool failed = fileReply_->error() != QNetworkReply::NoError;
    const auto error = fileReply_->errorString();
    fileReply_->deleteLater();
    fileReply_ = nullptr;

    if (failed)
    {
        file_->cancelWriting();
        delete file_;
        file_ = nullptr;
        emit fileFinished(false, error);
        return;
    }

    const bool committed = file_->commit();
    const auto commitError = file_->errorString();
    delete file_;
    file_ = nullptr;
    emit fileFinished(committed, committed ? QString() : commitError);
}
