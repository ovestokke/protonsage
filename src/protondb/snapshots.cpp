#include "protondb/snapshots.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QEventLoop>
#include <QDebug>
#include <algorithm>

namespace ProtonSage {

// ── Month abbreviation parser ─────────────────────────────────────────

static const QMap<QString, int>& monthAbbrevMap()
{
    static const QMap<QString, int> m = []() {
        QMap<QString, int> map;
        auto add = [&](const char* name, int val) {
            map.insert(QString::fromLatin1(name), val);
        };
        add("jan", 1); add("feb", 2); add("mar", 3);
        add("apr", 4); add("may", 5); add("jun", 6);
        add("jul", 7); add("aug", 8); add("sep", 9);
        add("oct", 10); add("nov", 11); add("dec", 12);
        return map;
    }();
    return m;
}

// ── parseSnapshotFilename ─────────────────────────────────────────────

QDate parseSnapshotFilename(const QString& filename)
{
    static const QRegularExpression re(
        QStringLiteral("^reports_([a-z]{3})(\\d{1,2})_(\\d{4})\\.tar\\.gz$"),
        QRegularExpression::CaseInsensitiveOption);

    const auto match = re.match(filename.trimmed());
    if (!match.hasMatch())
        return {};

    const QString monthStr = match.captured(1).toLower();
    if (!monthAbbrevMap().contains(monthStr))
        return {};

    bool dayOk = false, yearOk = false;
    int day = match.captured(2).toInt(&dayOk);
    int year = match.captured(3).toInt(&yearOk);
    if (!dayOk || !yearOk)
        return {};

    int month = monthAbbrevMap().value(monthStr, 0);
    if (month < 1 || month > 12 || day < 1 || day > 31)
        return {};

    QDate date(year, month, day);
    // Validate that the parsed components survive round-trip through QDate
    // (catches invalid combos like Feb 30).
    if (date.year() != year || date.month() != month || date.day() != day)
        return {};

    return date;
}

// ── selectLatestSnapshot ──────────────────────────────────────────────

Snapshot selectLatestSnapshot(const QStringList& filenames)
{
    Snapshot latest;
    for (const QString& fn : filenames) {
        QDate date = parseSnapshotFilename(fn);
        if (!date.isValid())
            continue;

        bool isNewer = !latest.date.isValid()
                       || date > latest.date
                       || (date == latest.date && fn > latest.filename);
        if (isNewer) {
            latest.filename = fn;
            latest.date = date;
            latest.url = kReportsRawBaseURL + u'/' + fn;
        }
    }
    return latest;
}

// ── sortSnapshotsNewestFirst ──────────────────────────────────────────

void sortSnapshotsNewestFirst(QList<Snapshot>& snapshots)
{
    std::sort(snapshots.begin(), snapshots.end(),
              [](const Snapshot& a, const Snapshot& b) {
                  if (a.date == b.date)
                      return a.filename > b.filename;
                  return a.date > b.date;
              });
}

// ── GitHub API helpers ────────────────────────────────────────────────

static QByteArray syncGet(QNetworkAccessManager& nam, const QUrl& url,
                           QString* error = nullptr)
{
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent",
                     "ProtonSage/0.2 (+https://github.com/bdefore/protondb-data)");

    QNetworkReply* reply = nam.get(req);

    // Synchronous wait using a local event loop.
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        if (error)
            *error = QStringLiteral("GitHub API error: %1").arg(reply->errorString());
        reply->deleteLater();
        return {};
    }

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status > 299) {
        if (error)
            *error = QStringLiteral("GitHub API returned HTTP %1").arg(status);
        reply->deleteLater();
        return {};
    }

    QByteArray body = reply->readAll();
    reply->deleteLater();
    return body;
}

// ── listSnapshotsFromGitHub ────────────────────────────────────────────

QList<Snapshot> listSnapshotsFromGitHub(QNetworkAccessManager& nam)
{
    QString err;
    QByteArray body = syncGet(nam, QUrl(kReportsContentsAPI), &err);
    if (body.isEmpty()) {
        qWarning() << "listSnapshotsFromGitHub:" << err;
        return {};
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
    if (doc.isNull() || !doc.isArray()) {
        qWarning() << "listSnapshotsFromGitHub: JSON parse error —"
                   << parseErr.errorString();
        return {};
    }

    QList<Snapshot> snapshots;
    QJsonArray items = doc.array();
    for (const QJsonValue& val : items) {
        QJsonObject obj = val.toObject();

        // Only process files (and entries without explicit type).
        QString type = obj[u"type"].toString();
        if (!type.isEmpty() && type != u"file")
            continue;

        QString name = obj[u"name"].toString();
        QDate date = parseSnapshotFilename(name);
        if (!date.isValid())
            continue;

        Snapshot snap;
        snap.filename = name;
        snap.date = date;
        snap.size = static_cast<qint64>(obj[u"size"].toDouble());

        // Prefer download_url; fall back to raw.githubusercontent.com.
        QString dw = obj[u"download_url"].toString();
        if (!dw.isEmpty())
            snap.url = dw;
        else
            snap.url = kReportsRawBaseURL + u'/' + name;

        snapshots.append(snap);
    }

    sortSnapshotsNewestFirst(snapshots);
    return snapshots;
}

// ── latestSnapshotFromGitHub ──────────────────────────────────────────

Snapshot latestSnapshotFromGitHub(QNetworkAccessManager& nam)
{
    QList<Snapshot> snapshots = listSnapshotsFromGitHub(nam);
    if (snapshots.isEmpty()) {
        qWarning() << "latestSnapshotFromGitHub: no snapshots found";
        return {};
    }
    return snapshots.first();
}

} // namespace ProtonSage
