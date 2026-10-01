/***************************************************************************
  remotesource.cpp
  ----------------
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

#include "remotesource.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QUrl>
#include <QUrlQuery>

#include "defs.h"
#include "globalsettings.h"
#include "widget_utils.h"

namespace {

QString current_drive;

// Cache path handed out by listFiles -> the link to download it from
QHash<QString, QString>& registry()
{
    static QHash<QString, QString> map;
    return map;
}

// Download URL -> where it was downloaded to. A file reached twice - through
// two settings, or a setting and a folder listing - crosses the network once
// and is copied locally the second time.
QHash<QString, QString>& downloaded()
{
    static QHash<QString, QString> map;
    return map;
}

QNetworkAccessManager* nam()
{
    // Owns the cookie jar, which the Dropbox listing needs across requests
    static auto* manager = new QNetworkAccessManager(qApp);
    return manager;
}

// Every provider host replaced by EDDYFLOW_REMOTE_BASE when that is set,
// the way the engine does, so the same local test server serves both
QUrl effective(const QUrl& url)
{
    const auto base = qEnvironmentVariable("EDDYFLOW_REMOTE_BASE");
    if (base.isEmpty()) { return url; }
    QUrl swapped(base);
    swapped.setPath(url.path());
    swapped.setQuery(url.query());
    return swapped;
}

bool request(const QUrl& url, const QByteArray* form, QByteArray* body,
             QString* error)
{
    QNetworkRequest req(effective(url));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(120000);

    QNetworkReply* reply = nullptr;
    if (form)
    {
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
        reply = nam()->post(req, *form);
    }
    else
    {
        reply = nam()->get(req);
    }

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool ok = reply->error() == QNetworkReply::NoError;
    *body = reply->readAll();
    if (!ok && error)
    {
        *error = status > 0
            ? QObject::tr("The server answered HTTP %1 (%2).").arg(status).arg(reply->errorString())
            : reply->errorString();
    }
    reply->deleteLater();
    return ok;
}

bool looksLikeHtml(const QByteArray& data)
{
    const auto head = data.left(64).trimmed().toLower();
    return head.startsWith("<!doctype html") || head.startsWith("<html");
}

QString decodeHtml(QString s)
{
    s.replace(QLatin1String("&amp;"), QLatin1String("&"));
    s.replace(QLatin1String("&#39;"), QLatin1String("'"));
    s.replace(QLatin1String("&quot;"), QLatin1String("\""));
    s.replace(QLatin1String("&lt;"), QLatin1String("<"));
    s.replace(QLatin1String("&gt;"), QLatin1String(">"));
    return s;
}

QString stripFragment(const QString& link)
{
    const auto hash = link.indexOf(QLatin1Char('#'));
    return hash < 0 ? link : link.left(hash);
}

QString fragmentItem(const QString& link, const QString& key)
{
    const auto hash = link.indexOf(QLatin1Char('#'));
    if (hash < 0) { return {}; }
    const QUrlQuery items(link.mid(hash + 1));
    return items.queryItemValue(key, QUrl::FullyDecoded);
}

QString googleId(const QString& link)
{
    static const QRegularExpression re(
        QStringLiteral("(?:/folders/|/file/d/|[?&]id=)([A-Za-z0-9_-]+)"));
    const auto m = re.match(stripFragment(link));
    return m.hasMatch() ? m.captured(1) : QString();
}

struct DropboxLink
{
    QString key;
    QString hash;
    QString sub;
    QString rlkey;
};

bool parseDropbox(const QString& link, DropboxLink* d)
{
    const QUrl url(stripFragment(link));
    const auto parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    // scl / fo / <key> / <hash> [/ sub path...]
    if (parts.size() < 4 || parts.at(0) != QLatin1String("scl")
        || parts.at(1) != QLatin1String("fo"))
    {
        return false;
    }
    d->key = parts.at(2);
    d->hash = parts.at(3);
    d->sub = parts.mid(4).join(QLatin1Char('/'));
    d->rlkey = QUrlQuery(url).queryItemValue(QStringLiteral("rlkey"));
    return !d->key.isEmpty() && !d->hash.isEmpty();
}

bool listGoogle(const QString& id, QList<RemoteSource::Entry>* entries, QString* error)
{
    QUrl url(QStringLiteral("https://drive.google.com/embeddedfolderview"));
    url.setQuery(QStringLiteral("id=") + id);
    QByteArray body;
    if (!request(url, nullptr, &body, error)) { return false; }

    const auto page = QString::fromUtf8(body);
    if (!page.contains(QLatin1String("flip-entries")))
    {
        *error = QObject::tr("Google Drive did not return a folder listing.");
        return false;
    }

    static const QRegularExpression entryRe(
        QStringLiteral("class=\"flip-entry\" id=\"entry-([^\"]+)\""));
    static const QRegularExpression titleRe(
        QStringLiteral("flip-entry-title\">([^<]*)<"));
    static const QRegularExpression dateRe(
        QStringLiteral("flip-entry-last-modified\"><div>([^<]*)<"));

    QList<QRegularExpressionMatch> matches;
    auto it = entryRe.globalMatch(page);
    while (it.hasNext()) { matches << it.next(); }

    for (int i = 0; i < matches.size(); ++i)
    {
        const auto start = matches.at(i).capturedEnd();
        const auto end = i + 1 < matches.size() ? matches.at(i + 1).capturedStart()
                                                : page.size();
        const auto segment = page.mid(start, end - start);
        const auto entryId = matches.at(i).captured(1);

        RemoteSource::Entry e;
        e.isDir = segment.contains(QLatin1String("/drive/folders/"));
        e.name = decodeHtml(titleRe.match(segment).captured(1));
        e.modified = decodeHtml(dateRe.match(segment).captured(1));
        e.link = e.isDir
            ? QStringLiteral("https://drive.google.com/drive/folders/") + entryId
            : QStringLiteral("https://drive.google.com/file/d/%1/view").arg(entryId);
        if (!e.name.isEmpty()) { entries->append(e); }
    }
    return true;
}

bool listDropbox(const QString& link, QList<RemoteSource::Entry>* entries, QString* error)
{
    DropboxLink d;
    if (!parseDropbox(link, &d))
    {
        *error = QObject::tr("This is not a Dropbox folder link (it should contain /scl/fo/).");
        return false;
    }

    // The page sets the CSRF cookie the listing request must repeat
    const QUrl page(stripFragment(link));
    QByteArray body;
    if (!request(page, nullptr, &body, error)) { return false; }
    QString token;
    const auto cookies = nam()->cookieJar()->cookiesForUrl(effective(page));
    for (const auto& c : cookies)
    {
        if (c.name() == "t") { token = QString::fromUtf8(c.value()); }
    }
    if (token.isEmpty())
    {
        *error = QObject::tr("Dropbox listing format not recognised (no session token).");
        return false;
    }

    QString voucher;
    for (int pages = 0; pages < 1000; ++pages)
    {
        QUrlQuery form;
        form.addQueryItem(QStringLiteral("is_xhr"), QStringLiteral("true"));
        form.addQueryItem(QStringLiteral("t"), token);
        form.addQueryItem(QStringLiteral("link_key"), d.key);
        form.addQueryItem(QStringLiteral("link_type"), QStringLiteral("c"));
        form.addQueryItem(QStringLiteral("secure_hash"), d.hash);
        form.addQueryItem(QStringLiteral("sub_path"), d.sub);
        form.addQueryItem(QStringLiteral("rlkey"), d.rlkey);
        if (!voucher.isEmpty()) { form.addQueryItem(QStringLiteral("voucher"), voucher); }
        const auto data = form.toString(QUrl::FullyEncoded).toUtf8();

        if (!request(QUrl(QStringLiteral("https://www.dropbox.com/list_shared_link_folder_entries")),
                     &data, &body, error))
        {
            return false;
        }
        const auto doc = QJsonDocument::fromJson(body);
        if (!doc.isObject() || !doc.object().contains(QLatin1String("entries")))
        {
            *error = QObject::tr("Dropbox listing format not recognised.");
            return false;
        }
        const auto obj = doc.object();
        for (const auto& v : obj.value(QLatin1String("entries")).toArray())
        {
            const auto o = v.toObject();
            RemoteSource::Entry e;
            e.name = o.value(QLatin1String("filename")).toString();
            e.isDir = o.value(QLatin1String("is_dir")).toBool();
            e.link = RemoteSource::canonical(o.value(QLatin1String("href")).toString());
            if (o.contains(QLatin1String("bytes")))
            {
                e.size = static_cast<qint64>(o.value(QLatin1String("bytes")).toDouble());
            }
            if (o.contains(QLatin1String("ts")))
            {
                e.modified = QDateTime::fromSecsSinceEpoch(
                    static_cast<qint64>(o.value(QLatin1String("ts")).toDouble()))
                    .toString(QStringLiteral("yyyy-MM-dd hh:mm"));
            }
            if (!e.name.isEmpty() && !e.link.isEmpty()) { entries->append(e); }
        }
        if (!obj.value(QLatin1String("has_more_entries")).toBool()) { return true; }
        voucher = obj.value(QLatin1String("next_request_voucher")).toString();
        if (voucher.isEmpty())
        {
            *error = QObject::tr("Dropbox listing format not recognised (no next page).");
            return false;
        }
    }
    *error = QObject::tr("The Dropbox folder has too many entries to list.");
    return false;
}

QString downloadUrl(const QString& link)
{
    switch (RemoteSource::provider(link))
    {
    case RemoteSource::Provider::GoogleDrive:
        return QStringLiteral("https://drive.usercontent.google.com/download?id=%1"
                              "&export=download&confirm=t").arg(googleId(link));
    case RemoteSource::Provider::Dropbox:
    {
        const auto c = RemoteSource::canonical(link);
        return c + (c.contains(QLatin1Char('?')) ? QStringLiteral("&dl=1")
                                                 : QStringLiteral("?dl=1"));
    }
    default:
        return {};
    }
}

bool collect(const QString& folderLink, const QString& localDir,
             const QRegularExpression& match, bool recurse, int depth,
             QStringList* files, QString* error)
{
    if (depth > 32) { return true; }
    QList<RemoteSource::Entry> entries;
    if (!RemoteSource::listFolder(folderLink, &entries, error)) { return false; }
    for (const auto& e : entries)
    {
        const auto local = localDir + QLatin1Char('/') + e.name;
        if (e.isDir)
        {
            if (recurse && !collect(e.link, local, match, recurse, depth + 1, files, error))
            {
                return false;
            }
        }
        else if (match.match(e.name).hasMatch())
        {
            registry().insert(QDir::cleanPath(local), e.link);
            files->append(QDir::cleanPath(local));
        }
    }
    return true;
}

QString shortHash(const QString& s)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
}

} // namespace

bool RemoteSource::isRemote(const QString& path)
{
    auto p = path.trimmed();
    if (p.startsWith(QLatin1Char('"'))) { p.remove(0, 1); }
    return p.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)
        || p.startsWith(QLatin1String("http://"), Qt::CaseInsensitive);
}

RemoteSource::Provider RemoteSource::provider(const QString& link)
{
    if (!isRemote(link)) { return Provider::None; }
    const auto l = stripFragment(link);
    if (l.contains(QLatin1String("google.com")) || l.contains(QLatin1String("/drive/folders/"))
        || l.contains(QLatin1String("/file/d/")))
    {
        return Provider::GoogleDrive;
    }
    if (l.contains(QLatin1String("dropbox.com")) || l.contains(QLatin1String("/scl/fo/"))
        || l.contains(QLatin1String("/scl/fi/")))
    {
        return Provider::Dropbox;
    }
    return Provider::None;
}

QString RemoteSource::cleanPath(const QString& path)
{
    return isRemote(path) ? path.trimmed() : QDir::cleanPath(path);
}

QString RemoteSource::canonical(const QString& link)
{
    const auto bare = stripFragment(link.trimmed());
    switch (provider(bare))
    {
    case Provider::GoogleDrive:
    {
        const auto id = googleId(bare);
        if (id.isEmpty()) { return bare; }
        if (bare.contains(QLatin1String("/file/d/")))
        {
            return QStringLiteral("https://drive.google.com/file/d/%1/view").arg(id);
        }
        if (bare.contains(QLatin1String("/folders/")))
        {
            return QStringLiteral("https://drive.google.com/drive/folders/") + id;
        }
        return bare;
    }
    case Provider::Dropbox:
    {
        QUrl url(bare);
        QUrlQuery q(url);
        q.removeAllQueryItems(QStringLiteral("st"));
        q.removeAllQueryItems(QStringLiteral("dl"));
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }
    default:
        return bare;
    }
}

QString RemoteSource::displayName(const QString& link)
{
    const auto name = provider(link) == Provider::Dropbox ? QObject::tr("Dropbox")
                                                          : QObject::tr("Google Drive");
    const auto rel = relPathOf(link);
    return rel.isEmpty() ? QStringLiteral("%1: %2").arg(name, canonical(link))
                         : QStringLiteral("%1: %2").arg(name, rel);
}

QString RemoteSource::withLocation(const QString& link, const QString& rootLink,
                                   const QString& relPath)
{
    return canonical(link)
        + QStringLiteral("#root=")
        + QString::fromLatin1(QUrl::toPercentEncoding(canonical(rootLink)))
        + QStringLiteral("&path=")
        + QString::fromLatin1(QUrl::toPercentEncoding(relPath, "/"));
}

QString RemoteSource::rootOf(const QString& link)
{
    return fragmentItem(link, QStringLiteral("root"));
}

QString RemoteSource::relPathOf(const QString& link)
{
    return fragmentItem(link, QStringLiteral("path"));
}

bool RemoteSource::isFileLink(const QString& link)
{
    return stripFragment(link).contains(QLatin1String("/file/d/"))
        || stripFragment(link).contains(QLatin1String("/scl/fi/"));
}

bool RemoteSource::listFolder(const QString& folderLink, QList<Entry>* entries,
                              QString* error)
{
    entries->clear();
    switch (provider(folderLink))
    {
    case Provider::GoogleDrive:
    {
        if (isFileLink(folderLink))
        {
            *error = QObject::tr("This link is to a file, not a folder.");
            return false;
        }
        const auto id = googleId(folderLink);
        if (id.isEmpty())
        {
            *error = QObject::tr("This Google Drive link names no folder.");
            return false;
        }
        return listGoogle(id, entries, error);
    }
    case Provider::Dropbox:
        return listDropbox(folderLink, entries, error);
    default:
        *error = QObject::tr("This is not a Google Drive or Dropbox link.");
        return false;
    }
}

bool RemoteSource::download(const QString& fileLink, const QString& dest, QString* error)
{
    const auto url = downloadUrl(fileLink);
    if (url.isEmpty())
    {
        *error = QObject::tr("This is not a Google Drive or Dropbox link.");
        return false;
    }
    QByteArray body;
    if (!request(QUrl(url), nullptr, &body, error)) { return false; }
    if (looksLikeHtml(body))
    {
        *error = QObject::tr("The provider sent a web page instead of the file - "
                             "a sign-in, permission or download quota page.");
        return false;
    }
    QDir().mkpath(QFileInfo(dest).absolutePath());
    const auto part = dest + QStringLiteral(".part");
    QFile out(part);
    if (!out.open(QIODevice::WriteOnly) || out.write(body) != body.size())
    {
        *error = QObject::tr("Could not write %1.").arg(QDir::toNativeSeparators(part));
        return false;
    }
    out.close();
    QFile::remove(dest);
    if (!QFile::rename(part, dest))
    {
        *error = QObject::tr("Could not write %1.").arg(QDir::toNativeSeparators(dest));
        return false;
    }
    return true;
}

QStringList RemoteSource::listFiles(const QString& folderLink, const QString& glob,
                                    bool recurse, QWidget* parent)
{
    const QRegularExpression match(
        QRegularExpression::wildcardToRegularExpression(glob),
        QRegularExpression::CaseInsensitiveOption);
    const auto localRoot = cacheDir() + QLatin1Char('/') + shortHash(canonical(folderLink));

    QStringList files;
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = collect(canonical(folderLink), localRoot, match, recurse, 0, &files, &error);
    QApplication::restoreOverrideCursor();
    if (!ok)
    {
        warn(parent, QObject::tr("Could not list the shared folder."), error);
        return {};
    }
    std::sort(files.begin(), files.end());
    return files;
}

QString RemoteSource::ensureLocal(const QString& pathOrLink, QWidget* parent)
{
    const auto key = QDir::cleanPath(pathOrLink);
    QString link;
    QString dest;
    if (registry().contains(key))
    {
        if (QFile::exists(key)) { return key; }
        link = registry().value(key);
        dest = key;
    }
    else if (isRemote(pathOrLink))
    {
        link = pathOrLink;
        auto name = QFileInfo(relPathOf(link)).fileName();
        if (name.isEmpty()) { name = QStringLiteral("file"); }
        dest = cacheDir() + QStringLiteral("/files/") + shortHash(canonical(link))
             + QLatin1Char('/') + name;
        if (QFile::exists(dest)) { return dest; }
    }
    else
    {
        return pathOrLink;
    }

    const auto url = downloadUrl(link);
    const auto have = downloaded().value(url);
    if (!have.isEmpty() && QFile::exists(have))
    {
        QDir().mkpath(QFileInfo(dest).absolutePath());
        if (QFile::copy(have, dest)) { return dest; }
    }

    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = download(link, dest, &error);
    QApplication::restoreOverrideCursor();
    if (!ok)
    {
        warn(parent, QObject::tr("Could not download %1 from the shared drive.")
                         .arg(QFileInfo(dest).fileName()), error);
        return {};
    }
    downloaded().insert(url, dest);
    return dest;
}

void RemoteSource::warn(QWidget* parent, const QString& what, const QString& detail)
{
    WidgetUtils::warning(parent ? parent : QApplication::activeWindow(),
                         QObject::tr("Remote Drive"),
                         what,
                         detail + QStringLiteral("\n\n")
                         + QObject::tr("Is the folder shared with \"Anyone with the link\", "
                                       "and can this computer reach the internet?"));
}

QString RemoteSource::currentDrive()
{
    return current_drive;
}

void RemoteSource::setCurrentDrive(const QString& rootLink)
{
    current_drive = canonical(rootLink);
    auto recent = recentDrives();
    recent.removeAll(current_drive);
    recent.prepend(current_drive);
    while (recent.size() > 10) { recent.removeLast(); }
    GlobalSettings::setAppPersistentSettings(Defs::CONFGROUP_WINDOW,
                                             QStringLiteral("/remote_drives"),
                                             recent);
}

void RemoteSource::clearCurrentDrive()
{
    current_drive.clear();
}

void RemoteSource::adoptDriveFrom(const QStringList& values)
{
    for (const auto& v : values)
    {
        if (!isRemote(v)) { continue; }
        const auto root = rootOf(v);
        current_drive = canonical(root.isEmpty() ? v : root);
        return;
    }
}

QStringList RemoteSource::recentDrives()
{
    return GlobalSettings::getAppPersistentSettings(Defs::CONFGROUP_WINDOW,
                                                    QStringLiteral("/remote_drives"))
        .toStringList();
}

QString RemoteSource::cacheDir()
{
    // Removed when the application exits
    static QTemporaryDir dir(QDir::tempPath() + QStringLiteral("/eddyflow_remote_XXXXXX"));
    return dir.path();
}
