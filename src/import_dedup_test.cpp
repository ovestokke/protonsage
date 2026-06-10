#include <QCoreApplication>
#include <QBuffer>
#include <QTemporaryDir>
#include <QFile>
#include <QDate>
#include <QByteArray>
#include <cstdio>
#include <cstring>
#include <zlib.h>

#include "storage/database.h"
#include "protondb/import.h"

#define LOG(fmt, ...) do { fprintf(stderr, fmt "\n", ##__VA_ARGS__); fflush(stderr); } while(0)

static QByteArray gzipData(const QByteArray& input) {
    z_stream s{};
    if (deflateInit2(&s, Z_BEST_SPEED, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return {};

    QByteArray out;
    out.resize(compressBound(input.size()) + 32);
    s.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.constData()));
    s.avail_in = static_cast<uInt>(input.size());
    s.next_out = reinterpret_cast<Bytef*>(out.data());
    s.avail_out = static_cast<uInt>(out.size());

    int ret = deflate(&s, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&s);
        return {};
    }
    out.resize(static_cast<int>(s.total_out));
    deflateEnd(&s);
    return out;
}

static QByteArray tarGzWithReports(const QByteArray& json) {
    QByteArray tar;
    tar.resize(512);
    tar.fill('\0');

    const QByteArray name = "reports_piiremoved.json";
    std::memcpy(tar.data(), name.constData(), name.size());
    char sizeField[12]{};
    std::snprintf(sizeField, sizeof(sizeField), "%011o", static_cast<unsigned int>(json.size()));
    std::memcpy(tar.data() + 124, sizeField, 11);
    tar[156] = '0';

    tar.append(json);
    int padding = (512 - (json.size() % 512)) % 512;
    tar.append(QByteArray(padding, '\0'));
    tar.append(QByteArray(1024, '\0'));
    return gzipData(tar);
}

static QByteArray reportJson(int appId, const QString& title, const char* verdict = "yes") {
    return QString(R"([
  {
    "app": { "steam": { "appId": "%1" }, "title": "%2" },
    "responses": {
      "verdict": "%3",
      "opens": "yes",
      "installs": "yes",
      "startsPlay": "yes",
      "protonVersion": "Proton Experimental"
    },
    "timestamp": 1710000000,
    "systemInfo": { "gpu": "NVIDIA GeForce RTX 4070", "os": "Test Linux", "ram": "32 GB" }
  }
])").arg(appId).arg(title).arg(verdict).toUtf8();
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    int passed = 0, failed = 0;
    auto check = [&](bool ok, const char* name) {
        if (ok) { ++passed; LOG("  PASS: %s", name); }
        else { ++failed; LOG("  FAIL: %s", name); }
    };

    LOG("=== ProtonSage Import Dedup Test ===");

    QTemporaryDir dir;
    check(dir.isValid(), "Temp dir created");
    auto dbOpt = ProtonSage::Database::open(dir.filePath("protonsage.db"));
    check(dbOpt.has_value(), "Database::open(temp)");
    if (!dbOpt) return 1;
    auto& db = *dbOpt;

    auto runImport = [&](const QByteArray& tgz, const QString& filename) {
        QBuffer buf;
        buf.setData(tgz);
        buf.open(QIODevice::ReadOnly);
        ProtonSage::SnapshotImportMeta meta;
        meta.snapshotFilename = filename;
        meta.snapshotDate = QDate(2026, 6, 1);
        meta.sourceUrl = QStringLiteral("file://test/") + filename;
        QString error;
        return ProtonSage::importSnapshot(db, buf, meta, &error);
    };

    QByteArray first = tarGzWithReports(reportJson(999001, "Dedup Test Game"));
    check(!first.isEmpty(), "First tar.gz built");
    auto r1 = runImport(first, "reports_jun1_2026.tar.gz");
    check(r1.reportsImported == 1, "First import inserts one report");
    check(db.status().reportCount == 1, "DB has one report after first import");
    check(db.reportCountByAppId(999001) == 1, "First app has one report");

    auto r2 = runImport(first, "reports_jun1_2026.tar.gz");
    check(r2.reportsImported == 1, "Second same snapshot imports one replacement report");
    check(db.status().reportCount == 1, "Same snapshot does not duplicate reports");
    check(db.reportCountByAppId(999001) == 1, "First app still has one report");

    QByteArray second = tarGzWithReports(reportJson(999002, "Replacement Snapshot Game"));
    check(!second.isEmpty(), "Second tar.gz built");
    auto r3 = runImport(second, "reports_jul1_2026.tar.gz");
    check(r3.reportsImported == 1, "New snapshot imports one report");
    check(db.status().reportCount == 1, "New snapshot replaces old ProtonDB reports");
    check(db.reportCountByAppId(999001) == 0, "Old snapshot report removed");
    check(db.reportCountByAppId(999002) == 1, "New snapshot report present");
    check(db.lookupGame(999001) == std::nullopt, "Orphan old game purged");
    check(db.lookupGame(999002).has_value(), "New game present");

    LOG("========== RESULTS ==========");
    LOG("%d passed, %d failed", passed, failed);
    return failed ? 1 : 0;
}
