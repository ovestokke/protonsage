#include "protondb/import.h"
#include "protondb/snapshots.h"
#include "core/models.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QTimeZone>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QDebug>
#include <zlib.h>

#include <cstring>
#include <algorithm>
#include <initializer_list>

namespace ProtonSage {

// ══════════════════════════════════════════════════════════════════════
// Internal helpers (anonymous namespace)
// ══════════════════════════════════════════════════════════════════════

namespace {

// ── Constants ─────────────────────────────────────────────────────────

const char kReportsJSONName[] = "reports_piiremoved.json";

// ── Gzip decompression ───────────────────────────────────────────────

static QByteArray gunzip(const QByteArray& compressed)
{
    if (compressed.isEmpty())
        return {};

    z_stream strm{};
    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;

    int ret = inflateInit2(&strm, MAX_WBITS + 16); // gzip auto-detect
    if (ret != Z_OK)
        return {};

    const int kChunk = 64 * 1024;
    QByteArray decompressed;
    decompressed.reserve(compressed.size() * 4);

    strm.avail_in = static_cast<uInt>(compressed.size());
    strm.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.constData()));

    do {
        char buf[kChunk];
        strm.avail_out = kChunk;
        strm.next_out  = reinterpret_cast<Bytef*>(buf);

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            return {};
        }
        decompressed.append(buf, kChunk - strm.avail_out);
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    return decompressed;
}

// ── Tar header parsing ───────────────────────────────────────────────

struct TarHeader {
    char name[100]{};
    char sizeStr[12]{};  // octal at offset 124
    char typeflag{};     // '0' or '\0' = regular, '5' = dir
};

static TarHeader parseTarHeader(const char* block)
{
    TarHeader hdr;
    std::memcpy(hdr.name, block, 100);
    std::memcpy(hdr.sizeStr, block + 124, 12);
    hdr.typeflag = block[156];
    return hdr;
}

static qint64 parseTarSize(const char* octal)
{
    qint64 size = 0;
    for (int i = 0; i < 12 && octal[i] && octal[i] != ' '; ++i) {
        if (octal[i] >= '0' && octal[i] <= '7')
            size = (size << 3) | (octal[i] - '0');
        else
            break;
    }
    return size;
}

/** Extract reports_piiremoved.json from a raw (decompressed) tar archive. */
static QByteArray extractReportsJSON(const QByteArray& tarData)
{
    const int kBlock = 512;
    const char* data = tarData.constData();
    qint64 len = tarData.size();
    qint64 pos = 0;

    while (pos + kBlock <= len) {
        bool allZero = true;
        for (int i = 0; i < kBlock; ++i) {
            if (data[pos + i] != 0) { allZero = false; break; }
        }
        if (allZero) break;

        TarHeader hdr = parseTarHeader(data + pos);
        qint64 fileSize = parseTarSize(hdr.sizeStr);
        qint64 paddedSize = ((fileSize + kBlock - 1) / kBlock) * kBlock;
        pos += kBlock;

        if (pos + paddedSize > len)
            break;
        if (hdr.typeflag != '0' && hdr.typeflag != '\0') {
            pos += paddedSize;
            continue;
        }

        const char* slash = std::strrchr(hdr.name, '/');
        const char* baseName = slash ? slash + 1 : hdr.name;
        if (std::strcmp(baseName, kReportsJSONName) == 0)
            return QByteArray(data + pos, static_cast<int>(fileSize));

        pos += paddedSize;
    }
    return {};
}

// ── JSON field lookup helpers (all take const char* init lists) ──────

static bool keyEquals(const QString& a, const QString& b)
{
    return QString::compare(a, b, Qt::CaseInsensitive) == 0;
}

/** Convert a C-string to QString via fromUtf8. */
static inline QString _S(const char* s) { return QString::fromUtf8(s); }

/** Build a QStringList from an initializer_list of C-strings. */
static QStringList keys(std::initializer_list<const char*> k)
{
    QStringList out;
    out.reserve(static_cast<int>(k.size()));
    for (const char* s : k)
        out.append(QString::fromUtf8(s));
    return out;
}

static QJsonValue firstValue(const QJsonObject& obj, const QStringList& keys_)
{
    for (const QString& key : keys_) {
        auto it = obj.find(key);
        if (it != obj.end() && !it->isUndefined())
            return *it;
    }
    for (const QString& key : keys_) {
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (keyEquals(it.key(), key) && !it->isUndefined())
                return *it;
        }
    }
    return {};
}

static QJsonObject firstObject(const QJsonObject& obj, const QStringList& keys_)
{
    QJsonValue v = firstValue(obj, keys_);
    return v.isObject() ? v.toObject() : QJsonObject{};
}

static QString firstString(const QJsonObject& obj, const QStringList& keys_)
{
    QJsonValue v = firstValue(obj, keys_);
    if (v.isString()) return v.toString().trimmed();
    if (v.isDouble()) return QString::number(v.toDouble(), 'f', -1);
    if (v.isBool()) return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return {};
}

static int firstInt(const QJsonObject& obj, const QStringList& keys_, bool* ok = nullptr)
{
    QJsonValue v = firstValue(obj, keys_);
    if (v.isDouble()) { if (ok) *ok = true; return static_cast<int>(v.toDouble()); }
    if (v.isString()) {
        bool p = false;
        int val = v.toString().trimmed().toInt(&p);
        if (ok) *ok = p;
        return p ? val : 0;
    }
    if (ok) *ok = false;
    return 0;
}

static double firstDouble(const QJsonObject& obj, const QStringList& keys_)
{
    QJsonValue v = firstValue(obj, keys_);
    if (v.isDouble()) return v.toDouble();
    if (v.isString()) {
        QString s = v.toString().trimmed();
        if (s.endsWith(QLatin1String("GB"), Qt::CaseInsensitive))
            s.chop(2);
        s = s.trimmed();
        bool ok = false;
        double d = s.toDouble(&ok);
        return ok ? d : 0.0;
    }
    return 0.0;
}

static QString firstNonEmpty(std::initializer_list<QString> values)
{
    for (const QString& v : values) {
        if (!v.trimmed().isEmpty())
            return v.trimmed();
    }
    return {};
}

// ── GPU vendor detection ────────────────────────────────────────────

static QString detectGPUVendor(const QString& text)
{
    QString lower = text.toLower();
    if (lower.contains(QLatin1String("nvidia"))
        || lower.contains(QLatin1String("geforce"))
        || lower.contains(QLatin1String("rtx"))
        || lower.contains(QLatin1String("gtx")))
        return QStringLiteral("NVIDIA");
    if (lower.contains(QLatin1String("amd"))
        || lower.contains(QLatin1String("radeon"))
        || lower.contains(QLatin1String("radv")))
        return QStringLiteral("AMD");
    if (lower.contains(QLatin1String("intel"))
        || lower.contains(QLatin1String("arc")))
        return QStringLiteral("Intel");
    return {};
}

// ── Freeform note heuristics ─────────────────────────────────────────

static bool looksLikeFreeformNote(const QString& value)
{
    QString s = value.trimmed();
    if (s.isEmpty()) return false;
    QString lower = s.toLower();
    if (lower == QLatin1String("yes") || lower == QLatin1String("no")
        || lower == QLatin1String("n/a") || lower == QLatin1String("na")
        || lower == QLatin1String("none") || lower == QLatin1String("default")
        || lower == QLatin1String("true") || lower == QLatin1String("false"))
        return false;
    return s.size() > 2;
}

static QStringList noteTextFromValue(const QJsonValue& value)
{
    if (value.isNull() || value.isUndefined())
        return {};
    if (value.isString()) {
        QString s = value.toString().trimmed();
        return looksLikeFreeformNote(s) ? QStringList{s} : QStringList{};
    }
    if (value.isObject()) {
        QJsonObject obj = value.toObject();
        QStringList parts;
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            for (const QString& text : noteTextFromValue(it.value()))
                parts.append(it.key() + QStringLiteral(": ") + text);
        }
        return parts;
    }
    if (value.isArray()) {
        QJsonArray arr = value.toArray();
        QStringList parts;
        for (const QJsonValue& item : arr)
            parts.append(noteTextFromValue(item));
        return parts;
    }
    return {};
}

static QStringList dedupeStrings(const QStringList& values)
{
    QMap<QString, QString> seen; // lower -> original
    QStringList out;
    for (const QString& v : values) {
        QString s = v.trimmed();
        if (s.isEmpty()) continue;
        QString key = s.toLower();
        if (seen.contains(key)) continue;
        seen[key] = s;
        out.append(s);
    }
    return out;
}

// ── Timestamp parsing ────────────────────────────────────────────────

static QDateTime parseReportTime(const QJsonValue& value)
{
    if (value.isDouble()) {
        double raw = value.toDouble();
        if (raw <= 0) return {};
        if (raw > 1e12)
            return QDateTime::fromMSecsSinceEpoch(
                static_cast<qint64>(raw), QTimeZone::UTC);
        return QDateTime::fromMSecsSinceEpoch(
            static_cast<qint64>(raw * 1000.0), QTimeZone::UTC);
    }
    if (!value.isString()) return {};

    QString s = value.toString().trimmed();
    if (s.isEmpty()) return {};

    bool ok = false;
    double raw = s.toDouble(&ok);
    if (ok && raw > 0) {
        if (raw > 1e12)
            return QDateTime::fromMSecsSinceEpoch(
                static_cast<qint64>(raw), QTimeZone::UTC);
        return QDateTime::fromMSecsSinceEpoch(
            static_cast<qint64>(raw * 1000.0), QTimeZone::UTC);
    }

    static const QStringList formats = {
        QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzzZ"),
        QStringLiteral("yyyy-MM-ddTHH:mm:ssZ"),
        QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz+HH:mm"),
        QStringLiteral("yyyy-MM-ddTHH:mm:ss+HH:mm"),
        QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz"),
        QStringLiteral("yyyy-MM-ddTHH:mm:ss"),
        QStringLiteral("yyyy-MM-dd HH:mm:ss"),
        QStringLiteral("yyyy-MM-dd"),
    };
    for (const QString& fmt : formats) {
        QDateTime dt = QDateTime::fromString(s, fmt);
        if (dt.isValid()) return dt.toUTC();
    }
    return {};
}

// ── Report ID generation ─────────────────────────────────────────────

static QString generatedReportID(const QString& sourceID, int appid,
                                  const QDateTime& timestamp, int index,
                                  const QString& rawJSON)
{
    QByteArray data = sourceID.toUtf8() + '\n'
                      + QByteArray::number(appid) + '\n'
                      + timestamp.toUTC().toString(Qt::ISODate).toUtf8() + '\n'
                      + QByteArray::number(index) + '\n'
                      + rawJSON.toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
    return QStringLiteral("generated:%1:%2:%3")
        .arg(appid)
        .arg(timestamp.toUTC().toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'")))
        .arg(QString::fromLatin1(hash.left(6).toHex()));
}

// ── Report field extraction ──────────────────────────────────────────

static int reportAppID(const QJsonObject& fields)
{
    // Top-level
    {
        bool ok = false;
        int id = firstInt(fields, keys({"appid", "appId", "steamAppId", "steam_appid"}), &ok);
        if (ok) return id;
    }
    // app.steam.appId
    QJsonObject app = firstObject(fields, keys({"app"}));
    if (!app.isEmpty()) {
        QJsonObject steam = firstObject(app, keys({"steam"}));
        if (!steam.isEmpty()) {
            bool ok = false;
            int id = firstInt(steam, keys({"appid", "appId", "steamAppId", "steam_appid"}), &ok);
            if (ok) return id;
        }
        bool ok = false;
        int id = firstInt(app, keys({"appid", "appId", "steamAppId", "steam_appid"}), &ok);
        if (ok) return id;
    }
    // responses
    QJsonObject resp = firstObject(fields, keys({"responses"}));
    if (!resp.isEmpty()) {
        bool ok = false;
        int id = firstInt(resp, keys({"answerToWhatGame", "appid", "appId",
                                       "steamAppId", "steam_appid"}), &ok);
        if (ok) return id;
    }
    return 0;
}

static QString reportTitle(const QJsonObject& fields)
{
    QString t = firstString(fields, keys({"title", "name", "gameTitle", "game_name"}));
    if (!t.isEmpty()) return t;
    QJsonObject app = firstObject(fields, keys({"app"}));
    if (!app.isEmpty())
        return firstString(app, keys({"title", "name", "gameTitle", "game_name"}));
    return {};
}

static QString reportVerdict(const QJsonObject& fields)
{
    QString v = firstString(fields, keys({"verdict", "summary"}));
    if (!v.isEmpty()) return v;
    QJsonObject r = firstObject(fields, keys({"responses"}));
    return r.isEmpty() ? QString{} : firstString(r, keys({"verdict", "summary"}));
}

static QString reportRating(const QJsonObject& fields)
{
    QString r = firstString(fields, keys({"rating", "tier"}));
    if (!r.isEmpty()) return r;
    QJsonObject resp = firstObject(fields, keys({"responses"}));
    return resp.isEmpty() ? QString{} : firstString(resp, keys({"rating", "tier", "verdict"}));
}

static QString reportLaunchOptions(const QJsonObject& fields)
{
    QString lo = firstString(fields, keys({"launchOptions", "launch_options",
                                            "launchOption", "launch_option"}));
    if (!lo.isEmpty()) return lo;
    QJsonObject resp = firstObject(fields, keys({"responses"}));
    return resp.isEmpty() ? QString{}
                          : firstString(resp, keys({"launchOptions", "launch_options",
                                                     "launchOption", "launch_option"}));
}

static QString reportProtonVersion(const QJsonObject& fields)
{
    QString pv = firstString(fields, keys({"protonVersion", "proton_version",
                                            "proton", "protonVersionName"}));
    if (!pv.isEmpty()) return pv;
    QJsonObject resp = firstObject(fields, keys({"responses"}));
    return resp.isEmpty() ? QString{}
                          : firstString(resp, keys({"protonVersion", "proton_version",
                                                     "proton", "protonVersionName"}));
}

static QString reportNotes(const QJsonObject& fields)
{
    QStringList parts;

    // Top-level notes/text/body/content/report
    QJsonValue notesVal = firstValue(fields, keys({"notes", "text", "body", "content", "report"}));
    if (!notesVal.isUndefined())
        parts.append(noteTextFromValue(notesVal));

    // Response-specific keys at top level and in responses sub-object.
    auto addNoteKeys = [&](const QJsonObject& obj) {
        static const QStringList noteKeys = {
            QStringLiteral("extra"),
            QStringLiteral("customizationsUsed"),
            QStringLiteral("audioFaults"),
            QStringLiteral("graphicalFaults"),
            QStringLiteral("inputFaults"),
            QStringLiteral("performanceFaults"),
            QStringLiteral("saveGameFaults"),
            QStringLiteral("significantBugs"),
            QStringLiteral("stabilityFaults"),
            QStringLiteral("windowingFaults")
        };
        for (const QString& key : noteKeys) {
            QString txt = firstString(obj, keys({key.toUtf8().constData()}));
            if (!txt.isEmpty() && looksLikeFreeformNote(txt))
                parts.append(key + QStringLiteral(": ") + txt);
        }
    };
    addNoteKeys(fields);

    // Responses sub-object
    QJsonObject resp = firstObject(fields, keys({"responses"}));
    if (!resp.isEmpty()) {
        QJsonValue rn = firstValue(resp, keys({"notes"}));
        if (!rn.isUndefined())
            parts.append(noteTextFromValue(rn));
        addNoteKeys(resp);
    }

    return dedupeStrings(parts).join(QChar('\n'));
}

// ── System info extraction ──────────────────────────────────────────

static ReportSystemInfo normalizeSystemInfo(const QJsonObject& systemObj,
                                             const QJsonObject& reportFields)
{
    ReportSystemInfo info;

    // GPU text (for vendor detection fallback)
    QString gpuText = firstNonEmpty({
        firstString(systemObj, keys({"gpu", "graphics", "videoCard", "video_card"})),
        firstString(reportFields, keys({"gpu"}))
    });

    // GPU vendor
    info.gpuVendor = firstNonEmpty({
        firstString(systemObj, keys({"gpuVendor", "gpu_vendor", "vendor"})),
        firstString(firstObject(systemObj, keys({"gpu", "graphics", "videoCard", "video_card"})),
                    keys({"vendor", "gpuVendor", "gpu_vendor"})),
        firstString(reportFields, keys({"gpuVendor", "gpu_vendor"})),
        detectGPUVendor(gpuText)
    });

    // GPU model
    QJsonObject gpuObj = firstObject(systemObj, keys({"gpu", "graphics", "videoCard", "video_card"}));
    info.gpuModel = firstNonEmpty({
        firstString(systemObj, keys({"gpuModel", "gpu_model", "gpuName", "gpu_name",
                                      "name", "model"})),
        firstString(gpuObj, keys({"name", "model", "gpuName", "gpu_model"})),
        firstString(reportFields, keys({"gpuModel", "gpu_model", "gpuName", "gpu_name"})),
        gpuText
    });

    // GPU driver
    info.gpuDriver = firstNonEmpty({
        firstString(systemObj, keys({"gpuDriver", "gpu_driver", "driver",
                                      "driverVersion", "driver_version"})),
        firstString(gpuObj, keys({"driver", "driverVersion", "driver_version"})),
        firstString(reportFields, keys({"gpuDriver", "gpu_driver", "driver",
                                         "driverVersion", "driver_version"}))
    });

    // CPU
    info.cpu = firstNonEmpty({
        firstString(systemObj, keys({"cpu", "cpuModel", "cpu_model", "processor"})),
        firstString(reportFields, keys({"cpu", "cpuModel", "cpu_model"}))
    });

    // RAM
    double ram = firstDouble(systemObj, keys({"ramGb", "ramGB", "ram_gb", "ram", "memory"}));
    if (ram <= 0)
        ram = firstDouble(reportFields, keys({"ramGb", "ramGB", "ram_gb"}));
    info.ramGb = ram;

    // Distro
    info.distro = firstNonEmpty({
        firstString(systemObj, keys({"distro", "distribution", "os", "osName", "os_name"})),
        firstString(reportFields, keys({"distro", "distribution", "os"}))
    });

    // Kernel
    info.kernel = firstNonEmpty({
        firstString(systemObj, keys({"kernel", "kernelVersion", "kernel_version"})),
        firstString(reportFields, keys({"kernel", "kernelVersion", "kernel_version"}))
    });

    // Desktop
    info.desktop = firstNonEmpty({
        firstString(systemObj, keys({"desktop", "desktopEnvironment", "desktop_environment",
                                      "session", "sessionType"})),
        firstString(reportFields, keys({"desktop", "desktopEnvironment", "session"}))
    });

    return info;
}

static bool systemInfoHasValues(const ReportSystemInfo& info)
{
    return !info.gpuVendor.isEmpty() || !info.gpuModel.isEmpty()
           || !info.gpuDriver.isEmpty() || !info.cpu.isEmpty()
           || info.ramGb > 0.0 || !info.distro.isEmpty()
           || !info.kernel.isEmpty() || !info.desktop.isEmpty();
}

// ── JSON traversal to collect raw report records ────────────────────

struct RawReportRecord {
    int appId = 0;
    QString title;
    QString sourceReportId;
    QJsonObject fields;
    QString rawJson;
};

static bool looksLikeReport(const QJsonObject& obj)
{
    static const QStringList timeKeys = {
        QStringLiteral("timestamp"), QStringLiteral("createdAt"),
        QStringLiteral("created_at"), QStringLiteral("date"), QStringLiteral("time")
    };
    for (const QString& key : timeKeys) {
        if (!firstValue(obj, keys({key.toUtf8().constData()})).isUndefined())
            return true;
    }

    static const QStringList noteKeys = {
        QStringLiteral("notes"), QStringLiteral("text"), QStringLiteral("body"),
        QStringLiteral("content"), QStringLiteral("report")
    };
    for (const QString& key : noteKeys) {
        QJsonValue v = firstValue(obj, keys({key.toUtf8().constData()}));
        if (v.isString() && !v.toString().trimmed().isEmpty())
            return true;
        if (v.isObject() && !v.toObject().isEmpty())
            return true;
    }

    static const QStringList ratingKeys = {
        QStringLiteral("rating"), QStringLiteral("verdict"), QStringLiteral("tier")
    };
    for (const QString& key : ratingKeys) {
        QJsonValue v = firstValue(obj, keys({key.toUtf8().constData()}));
        if (v.isString() && !v.toString().trimmed().isEmpty())
            return true;
    }

    return false;
}

static void collectFromValue(QList<RawReportRecord>& records, const QJsonValue& value,
                              int inheritedAppID, const QString& inheritedTitle,
                              const QString& inheritedID);

static void collectValue(QList<RawReportRecord>& records, const QJsonValue& value,
                          int inheritedAppID, const QString& inheritedTitle,
                          const QString& inheritedID)
{
    if (value.isArray()) {
        QJsonArray arr = value.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            QString itemID = inheritedID.isEmpty()
                ? QString::number(i)
                : inheritedID + QChar(':') + QString::number(i);
            collectFromValue(records, arr[i], inheritedAppID, inheritedTitle, itemID);
        }
        return;
    }
    if (!value.isObject())
        return;

    QJsonObject obj = value.toObject();
    int appid = inheritedAppID;
    int explicitAppID = reportAppID(obj);
    if (explicitAppID > 0) appid = explicitAppID;

    QString title = inheritedTitle;
    QString explicitTitle = reportTitle(obj);
    if (!explicitTitle.isEmpty()) title = explicitTitle;

    // "reports" key?
    QJsonValue reportsVal = firstValue(obj, keys({"reports"}));
    if (!reportsVal.isUndefined()) {
        if (reportsVal.isArray()) {
            QJsonArray arr = reportsVal.toArray();
            for (int i = 0; i < arr.size(); ++i) {
                QString itemID = inheritedID.isEmpty()
                    ? QString::number(i)
                    : inheritedID + QChar('/') + QString::number(i);
                collectFromValue(records, arr[i], appid, title, itemID);
            }
        } else if (reportsVal.isObject()) {
            QJsonObject robj = reportsVal.toObject();
            for (auto it = robj.begin(); it != robj.end(); ++it) {
                QString itemID = inheritedID.isEmpty()
                    ? it.key()
                    : inheritedID + QChar('/') + it.key();
                collectFromValue(records, it.value(), appid, title, itemID);
            }
        }
        return;
    }

    // Looks like a report?
    if (looksLikeReport(obj) || appid > 0) {
        QString id = firstString(obj, keys({"id", "reportId", "reportID", "uuid", "_id"}));
        if (id.isEmpty()) id = inheritedID;
        QString rawJson = QString::fromUtf8(
            QJsonDocument(obj).toJson(QJsonDocument::Compact));
        records.append({appid, title, id, obj, rawJson});
        return;
    }

    // Recurse into each key.
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        int keyAppID = appid;
        bool parsed = false;
        int maybeID = it.key().toInt(&parsed);
        if (parsed) keyAppID = maybeID;

        QString childID = inheritedID.isEmpty()
            ? it.key()
            : inheritedID + QChar(':') + it.key();
        collectFromValue(records, it.value(), keyAppID, title, childID);
    }
}

static void collectFromValue(QList<RawReportRecord>& records, const QJsonValue& value,
                              int inheritedAppID, const QString& inheritedTitle,
                              const QString& inheritedID)
{
    if (value.isArray()) {
        QJsonArray arr = value.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            QString itemID = inheritedID.isEmpty()
                ? QString::number(i)
                : inheritedID + QChar(':') + QString::number(i);
            collectValue(records, arr[i], inheritedAppID, inheritedTitle, itemID);
        }
    } else if (value.isObject()) {
        collectValue(records, value, inheritedAppID, inheritedTitle, inheritedID);
    }
}

// ── Normalize a raw record into a Report + Game ─────────────────────

struct NormalizedReport {
    Game game;
    Report report;
    QString sourceReportId;
    QString rawJson;
    ReportSystemInfo systemInfo;
    bool hasSystemInfo = false;
};

static NormalizedReport normalizeRecord(const RawReportRecord& raw,
                                         const QString& sourceID, int index)
{
    NormalizedReport nr;
    const QJsonObject& fields = raw.fields;

    int appid = raw.appId;
    int explicitID = reportAppID(fields);
    if (explicitID > 0) appid = explicitID;
    if (appid <= 0) { nr.report.appId = 0; return nr; }

    QDateTime ts = parseReportTime(
        firstValue(fields, keys({"timestamp", "createdAt", "created_at", "date", "time"})));
    if (!ts.isValid()) { nr.report.appId = 0; return nr; }

    QString title = firstNonEmpty({
        reportTitle(fields),
        raw.title,
        QStringLiteral("Steam App %1").arg(appid)
    });

    QString sourceReportId = firstNonEmpty({
        firstString(fields, keys({"id", "reportId", "reportID", "uuid", "_id"})),
        raw.sourceReportId
    });
    if (sourceReportId.isEmpty() || sourceReportId.toInt())
        sourceReportId = generatedReportID(sourceID, appid, ts, index, raw.rawJson);

    // System info
    QJsonValue sysVal = firstValue(fields, keys({"systemInfo", "system", "specs"}));
    bool hasSys = !sysVal.isUndefined();
    QJsonObject sysObj = sysVal.isObject() ? sysVal.toObject() : QJsonObject();
    ReportSystemInfo info = normalizeSystemInfo(sysObj, fields);
    info.rawJson = hasSys
        ? QString::fromUtf8(
            (sysVal.isObject()
                 ? QJsonDocument(sysVal.toObject())
                 : QJsonDocument(sysVal.toArray()))
                .toJson(QJsonDocument::Compact))
        : QString{};
    if (!hasSys && !systemInfoHasValues(info))
        info = ReportSystemInfo{};

    // System info map for Report.systemInfo
    QMap<QString, QString> sysInfoMap;
    if (hasSys) {
        QJsonObject sysObjAll = sysVal.isObject() ? sysVal.toObject() : QJsonObject{};
        for (auto it = sysObjAll.begin(); it != sysObjAll.end(); ++it) {
            if (it.value().isString())
                sysInfoMap[it.key()] = it.value().toString();
            else if (it.value().isDouble())
                sysInfoMap[it.key()] = QString::number(it.value().toDouble());
            else if (it.value().isBool())
                sysInfoMap[it.key()] = it.value().toBool()
                    ? QStringLiteral("true") : QStringLiteral("false");
        }
    }

    Report report;
    report.sourceReportId = sourceReportId;
    report.appId = appid;
    report.title = title;
    report.timestamp = ts;
    report.verdict = reportVerdict(fields);
    report.rating = reportRating(fields);
    report.notes = reportNotes(fields);
    report.launchOptions = reportLaunchOptions(fields);
    report.protonVersion = reportProtonVersion(fields);
    report.sourceId = sourceID;
    report.systemInfo = sysInfoMap;

    Game game;
    game.appId = appid;
    game.name = title;
    game.launcher = QStringLiteral("steam");

    nr.game = game;
    nr.report = report;
    nr.sourceReportId = sourceReportId;
    nr.rawJson = raw.rawJson.isEmpty() ? QStringLiteral("{}") : raw.rawJson;
    nr.systemInfo = info;
    nr.hasSystemInfo = hasSys || systemInfoHasValues(info);
    return nr;
}

// ── Normalize import metadata ──────────────────────────────────────

static bool normalizeMeta(SnapshotImportMeta& meta, QString& error)
{
    meta.sourceId = meta.sourceId.trimmed();
    meta.snapshotFilename = meta.snapshotFilename.trimmed();
    meta.sourceUrl = meta.sourceUrl.trimmed();
    meta.licenseNote = meta.licenseNote.trimmed();

    if (meta.snapshotFilename.isEmpty()) {
        error = QStringLiteral("import snapshot: snapshot filename is required");
        return false;
    }
    if (!meta.snapshotDate.isValid()) {
        QDate d = parseSnapshotFilename(meta.snapshotFilename);
        if (d.isValid()) {
            meta.snapshotDate = d;
        } else {
            error = QStringLiteral("import snapshot: snapshot date is required "
                                    "when filename date cannot be parsed");
            return false;
        }
    }
    if (meta.sourceUrl.isEmpty())
        meta.sourceUrl = kReportsRawBaseURL + QChar('/') + meta.snapshotFilename;
    if (meta.licenseNote.isEmpty())
        meta.licenseNote = dataLicenseNote();
    if (meta.sourceId.isEmpty())
        meta.sourceId = QStringLiteral("protondb-data:") + meta.snapshotFilename;

    return true;
}

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════
// Public API
// ══════════════════════════════════════════════════════════════════════

const QString& dataLicenseNote()
{
    static const QString s = QStringLiteral(
        "ProtonDB/protondb-data report data; ODbL/DbCL attribution: "
        "https://github.com/bdefore/protondb-data");
    return s;
}

ImportResult importSnapshot(Database& db, QIODevice& stream,
                             const SnapshotImportMeta& metaIn,
                             QString* error)
{
    ImportResult result;
    QString err;

    SnapshotImportMeta meta = metaIn;
    if (!normalizeMeta(meta, err)) {
        if (error) *error = err;
        return {};
    }

    QByteArray compressed = stream.readAll();
    if (compressed.isEmpty()) {
        if (error) *error = QStringLiteral("import snapshot: empty input stream");
        return {};
    }

    QByteArray tarData = gunzip(compressed);
    if (tarData.isEmpty()) {
        if (error) *error = QStringLiteral("import snapshot: gzip decompression failed");
        return {};
    }

    QByteArray jsonPayload = extractReportsJSON(tarData);
    if (jsonPayload.isEmpty()) {
        if (error)
            *error = QStringLiteral("import snapshot: archive does not contain %1")
                         .arg(QLatin1String(kReportsJSONName));
        return {};
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonPayload, &parseErr);
    if (doc.isNull()) {
        if (error)
            *error = QStringLiteral("import snapshot: JSON parse error — %1")
                         .arg(parseErr.errorString());
        return {};
    }

    QList<RawReportRecord> rawRecords;
    collectValue(rawRecords,
                 doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object()),
                 0, QString{}, QString{});
    if (rawRecords.isEmpty()) {
        if (error)
            *error = QStringLiteral("import snapshot: no report records found in %1")
                         .arg(QLatin1String(kReportsJSONName));
        return {};
    }

    result.sourceId = meta.sourceId;
    result.snapshotFilename = meta.snapshotFilename;

    QSqlDatabase sqlDb = QSqlDatabase::database(QStringLiteral("protonsage"));
    if (!sqlDb.isOpen()) {
        if (error) *error = QStringLiteral("import snapshot: database not open");
        return {};
    }
    if (!sqlDb.transaction()) {
        if (error)
            *error = QStringLiteral("import snapshot: begin transaction failed — %1")
                         .arg(sqlDb.lastError().text());
        return {};
    }

    bool txFailed = false;
    QSet<int> seenGames;

    do {
        db.upsertSource(meta.sourceId, QStringLiteral("protondb-data"),
                        meta.sourceUrl, meta.licenseNote);

        qint64 runId = db.createImportRun(
            meta.sourceId, meta.snapshotFilename,
            meta.snapshotDate, meta.sourceUrl, meta.licenseNote);
        result.importRunId = runId;
        if (runId <= 0) {
            err = QStringLiteral("import snapshot: createImportRun failed");
            txFailed = true;
            break;
        }

        for (int i = 0; i < rawRecords.size(); ++i) {
            NormalizedReport nr = normalizeRecord(rawRecords[i], meta.sourceId, i);
            if (nr.report.appId <= 0) {
                result.recordsSkipped++;
                continue;
            }

            db.upsertGame(nr.game);
            if (!seenGames.contains(nr.game.appId)) {
                seenGames.insert(nr.game.appId);
                result.gamesImported++;
            }

            qint64 reportId = db.insertReport(runId, nr.report, nr.rawJson);
            if (reportId <= 0) {
                err = QStringLiteral("import snapshot: insertReport returned invalid ID");
                txFailed = true;
                break;
            }
            result.reportsImported++;

            if (nr.hasSystemInfo) {
                db.upsertReportSystemInfo(reportId, nr.systemInfo);
                result.systemInfoImported++;
            }
        }
        if (txFailed) break;

        db.finishImportRun(runId, result.reportsImported, result.recordsSkipped);
    } while (false);

    if (txFailed) {
        sqlDb.rollback();
        if (error) *error = err;
        return {};
    }
    if (!sqlDb.commit()) {
        if (error)
            *error = QStringLiteral("import snapshot: commit failed — %1")
                         .arg(sqlDb.lastError().text());
        return {};
    }
    if (result.reportsImported == 0) {
        if (error)
            *error = QStringLiteral("import snapshot: no valid reports imported; "
                                     "skipped %1 records").arg(result.recordsSkipped);
        return {};
    }
    return result;
}

} // namespace ProtonSage
