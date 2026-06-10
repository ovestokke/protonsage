#include "database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>

namespace ProtonSage {

std::optional<Database> Database::open(const QString& path) {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "protonsage");
    db.setDatabaseName(path);
    if (!db.open()) {
        qWarning() << "Database open failed:" << db.lastError().text();
        return std::nullopt;
    }
    // WAL mode for concurrent reads
    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA foreign_keys=ON");

    Database d(db);
    d.ensureSchema();
    return d;
}

Database::~Database() {
    if (m_moved) return;
    m_db.close();
    QSqlDatabase::removeDatabase("protonsage");
}

void Database::ensureSchema() {
    QFile f(":/src/storage/schema.sql");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot read schema.sql from resources";
        return;
    }
    QString sql = f.readAll();

    // Split SQL into statements, respecting BEGIN/END blocks (used in triggers)
    QStringList statements;
    QString current;
    int depth = 0;
    for (const QString& line : sql.split('\n')) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith("--")) continue;

        for (int i = 0; i < trimmed.size(); ++i) {
            if (trimmed.mid(i).startsWith("BEGIN") &&
                (i == 0 || !trimmed[i-1].isLetter()) &&
                (i + 5 >= trimmed.size() || !trimmed[i+5].isLetter()))
                depth++;
            else if (trimmed.mid(i).startsWith("END") &&
                     (i == 0 || !trimmed[i-1].isLetter()) &&
                     (i + 3 >= trimmed.size() || !trimmed[i+3].isLetter()))
                depth--;
        }

        bool endsWithSemicolon = trimmed.endsWith(';');
        if (!current.isEmpty()) current += '\n';
        current += trimmed;

        if (endsWithSemicolon && depth <= 0) {
            // Remove trailing semicolon for QSqlQuery
            if (current.endsWith(';')) current.chop(1);
            statements.append(current.trimmed());
            current.clear();
        }
    }
    if (!current.trimmed().isEmpty()) {
        if (current.endsWith(';')) current.chop(1);
        statements.append(current.trimmed());
    }

    for (const QString& s : statements) {
        if (s.isEmpty()) continue;
        QSqlQuery q(m_db);
        if (!q.exec(s)) {
            qWarning() << "Schema error:" << q.lastError().text() << "\nSQL:" << s.left(200);
        }
    }
}

// ── Status ───────────────────────────────────────────────────────────

DataStatus Database::status() {
    DataStatus s;
    auto count = [this](const QString& tbl) -> int {
        QSqlQuery q(m_db);
        q.exec("SELECT COUNT(*) FROM " + tbl);
        return q.next() ? q.value(0).toInt() : 0;
    };
    s.sourceCount = count("sources");
    s.importRunCount = count("import_runs");
    s.gameCount = count("games");
    s.reportCount = count("reports");

    QSqlQuery q(m_db);
    q.exec("SELECT id, source_id, snapshot_filename, snapshot_date, source_url, license, "
           "started_at, finished_at, report_count, skipped_count "
           "FROM import_runs ORDER BY started_at DESC LIMIT 1");
    if (q.next()) {
        s.latestImport.id = q.value(0).toLongLong();
        s.latestImport.sourceId = q.value(1).toString();
        s.latestImport.snapshotFilename = q.value(2).toString();
        s.latestImport.snapshotDate = QDate::fromString(q.value(3).toString(), Qt::ISODate);
        s.latestImport.sourceUrl = q.value(4).toString();
        s.latestImport.license = q.value(5).toString();
        s.latestImport.startedAt = QDateTime::fromString(q.value(6).toString(), Qt::ISODate);
        s.latestImport.reportCount = q.value(8).toInt();
        s.latestImport.skippedCount = q.value(9).toInt();
    }
    return s;
}

// ── Sources ───────────────────────────────────────────────────────────

void Database::upsertSource(const QString& id, const QString& kind,
                             const QString& url, const QString& license) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO sources (id, kind, url, license, imported_at) "
              "VALUES (?, ?, ?, ?, datetime('now')) "
              "ON CONFLICT(id) DO UPDATE SET kind=excluded.kind, url=excluded.url, "
              "license=excluded.license, imported_at=excluded.imported_at");
    q.addBindValue(id); q.addBindValue(kind); q.addBindValue(url); q.addBindValue(license);
    q.exec();
}

qint64 Database::createImportRun(const QString& sourceId, const QString& snapshotFilename,
                                  const QDate& snapshotDate, const QString& sourceUrl,
                                  const QString& license) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO import_runs (source_id, snapshot_filename, snapshot_date, "
              "source_url, license, started_at) VALUES (?, ?, ?, ?, ?, datetime('now'))");
    q.addBindValue(sourceId); q.addBindValue(snapshotFilename);
    q.addBindValue(snapshotDate.toString(Qt::ISODate));
    q.addBindValue(sourceUrl); q.addBindValue(license);
    q.exec();
    return q.lastInsertId().toLongLong();
}

void Database::finishImportRun(qint64 id, int reportCount, int skippedCount) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE import_runs SET finished_at=datetime('now'), report_count=?, skipped_count=? WHERE id=?");
    q.addBindValue(reportCount); q.addBindValue(skippedCount); q.addBindValue(id);
    q.exec();
}

// ── Games ─────────────────────────────────────────────────────────────

void Database::upsertGame(const Game& game) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO games (appid, name, launcher) VALUES (?, ?, ?) "
              "ON CONFLICT(appid) DO UPDATE SET name=excluded.name, launcher=excluded.launcher");
    q.addBindValue(game.appId);
    q.addBindValue(game.name.isEmpty() ? QString("Steam App %1").arg(game.appId) : game.name);
    q.addBindValue(game.launcher.isEmpty() ? "steam" : game.launcher);
    q.exec();
}

std::optional<Game> Database::lookupGame(int appId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT appid, name, launcher FROM games WHERE appid=?");
    q.addBindValue(appId);
    if (!q.exec() || !q.next()) return std::nullopt;
    Game g;
    g.appId = q.value(0).toInt();
    g.name = q.value(1).toString();
    g.launcher = q.value(2).toString();
    return g;
}

QList<Game> Database::searchGames(const QString& query, int limit) {
    QList<Game> result;
    // FTS5 search
    QString ftsQuery;
    for (const QString& word : query.split(' ', Qt::SkipEmptyParts)) {
        if (!ftsQuery.isEmpty()) ftsQuery += " AND ";
        ftsQuery += "\"" + word + "\"";
    }
    if (ftsQuery.isEmpty()) return result;

    QSqlQuery q(m_db);
    q.prepare("SELECT g.appid, g.name, g.launcher FROM games_fts "
              "JOIN games AS g ON g.appid = games_fts.rowid "
              "WHERE games_fts MATCH ? ORDER BY bm25(games_fts) LIMIT ?");
    q.addBindValue(ftsQuery); q.addBindValue(qMin(limit, 100));
    q.exec();
    while (q.next()) {
        Game g;
        g.appId = q.value(0).toInt();
        g.name = q.value(1).toString();
        g.launcher = q.value(2).toString();
        result.append(g);
    }
    return result;
}

int Database::reportCountByAppId(int appId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM reports WHERE appid=?");
    q.addBindValue(appId);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

// ── Reports ───────────────────────────────────────────────────────────

qint64 Database::insertReport(qint64 importRunId, const Report& report, const QString& rawJson) {
    QString sourceReportId = report.sourceReportId;
    if (sourceReportId.isEmpty()) {
        sourceReportId = QString("%1:%2:%3").arg(report.sourceId).arg(report.appId)
                             .arg(report.timestamp.toString(Qt::ISODate));
    }
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO reports (import_run_id, source_id, source_report_id, appid, title, "
              "timestamp, verdict, rating, notes, launch_options, proton_version, raw_json) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(importRunId); q.addBindValue(report.sourceId); q.addBindValue(sourceReportId);
    q.addBindValue(report.appId); q.addBindValue(report.title);
    q.addBindValue(report.timestamp.toString(Qt::ISODate));
    q.addBindValue(report.verdict); q.addBindValue(report.rating); q.addBindValue(report.notes);
    q.addBindValue(report.launchOptions); q.addBindValue(report.protonVersion);
    q.addBindValue(rawJson.isEmpty() ? "{}" : rawJson);
    q.exec();
    return q.lastInsertId().toLongLong();
}

void Database::upsertReportSystemInfo(qint64 reportId, const ReportSystemInfo& info) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO report_system_info (report_id, gpu_vendor, gpu_model, gpu_driver, "
              "cpu, ram_gb, distro, kernel, desktop, raw_json) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
              "ON CONFLICT(report_id) DO UPDATE SET "
              "gpu_vendor=excluded.gpu_vendor, gpu_model=excluded.gpu_model, "
              "gpu_driver=excluded.gpu_driver, cpu=excluded.cpu, ram_gb=excluded.ram_gb, "
              "distro=excluded.distro, kernel=excluded.kernel, desktop=excluded.desktop, "
              "raw_json=excluded.raw_json");
    q.addBindValue(reportId); q.addBindValue(info.gpuVendor); q.addBindValue(info.gpuModel);
    q.addBindValue(info.gpuDriver); q.addBindValue(info.cpu); q.addBindValue(info.ramGb);
    q.addBindValue(info.distro); q.addBindValue(info.kernel); q.addBindValue(info.desktop);
    q.addBindValue(info.rawJson);
    q.exec();
}

QList<ReportRecord> Database::reportsByAppId(int appId) {
    QList<ReportRecord> result;
    QSqlQuery q(m_db);
    q.prepare(R"(
        SELECT r.id, r.import_run_id, r.source_id, r.source_report_id, r.appid, r.title,
               r.timestamp, r.verdict, r.rating, r.notes, r.launch_options, r.proton_version, r.raw_json,
               COALESCE(si.gpu_vendor,''), COALESCE(si.gpu_model,''), COALESCE(si.gpu_driver,''),
               COALESCE(si.cpu,''), COALESCE(si.ram_gb,0), COALESCE(si.distro,''),
               COALESCE(si.kernel,''), COALESCE(si.desktop,''), COALESCE(si.raw_json,'')
        FROM reports AS r
        LEFT JOIN report_system_info AS si ON si.report_id = r.id
        WHERE r.appid = ?
        ORDER BY r.timestamp DESC, r.id DESC
    )");
    q.addBindValue(appId);
    q.exec();
    while (q.next()) {
        ReportRecord rec;
        rec.id = q.value(0).toLongLong();
        rec.importRunId = q.value(1).toLongLong();
        rec.report.sourceId = q.value(2).toString();
        rec.sourceReportId = q.value(3).toString();
        rec.report.appId = q.value(4).toInt();
        rec.report.title = q.value(5).toString();
        rec.report.timestamp = QDateTime::fromString(q.value(6).toString(), Qt::ISODate);
        rec.report.verdict = q.value(7).toString();
        rec.report.rating = q.value(8).toString();
        rec.report.notes = q.value(9).toString();
        rec.report.launchOptions = q.value(10).toString();
        rec.report.protonVersion = q.value(11).toString();
        rec.rawJson = q.value(12).toString();
        rec.report.id = rec.id;
        rec.report.sourceReportId = rec.sourceReportId;

        rec.systemInfo.gpuVendor = q.value(13).toString();
        rec.systemInfo.gpuModel = q.value(14).toString();
        rec.systemInfo.gpuDriver = q.value(15).toString();
        rec.systemInfo.cpu = q.value(16).toString();
        rec.systemInfo.ramGb = q.value(17).toDouble();
        rec.systemInfo.distro = q.value(18).toString();
        rec.systemInfo.kernel = q.value(19).toString();
        rec.systemInfo.desktop = q.value(20).toString();
        rec.systemInfo.rawJson = q.value(21).toString();
        result.append(rec);
    }
    return result;
}

Database::GameRating Database::gameRating(int appId) {
    GameRating r;
    QSqlQuery q(m_db);
    // Last 90 days only — patches change everything
    q.prepare(R"(
        SELECT
            COUNT(*),
            SUM(CASE WHEN verdict='yes' THEN 1 ELSE 0 END)
        FROM reports WHERE appid=?
          AND timestamp > datetime('now', '-90 days')
    )");
    q.addBindValue(appId);
    if (q.exec() && q.next()) {
        r.total = q.value(0).toInt();
        r.yes = q.value(1).toInt();
        r.clean = r.yes; // same — verdict based
    }
    return r;
}

} // namespace ProtonSage
