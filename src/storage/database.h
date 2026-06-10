#pragma once
#include <QString>
#include <QSqlDatabase>
#include <QList>
#include <optional>
#include "core/models.h"

namespace ProtonSage {

struct ReportSystemInfo {
    QString gpuVendor, gpuModel, gpuDriver;
    QString cpu;
    double ramGb = 0;
    QString distro, kernel, desktop;
    QString rawJson;
};

struct ReportRecord {
    qint64 id = 0, importRunId = 0;
    QString sourceReportId, rawJson;
    Report report;
    ReportSystemInfo systemInfo;
};

class Database {
public:
    static std::optional<Database> open(const QString& path);
    ~Database();

    Database(Database&& other) noexcept : m_db(std::move(other.m_db)) { other.m_moved = true; }
    Database& operator=(Database&& other) noexcept {
        if (this != &other) { m_db = std::move(other.m_db); other.m_moved = true; }
        return *this;
    }
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Status
    DataStatus status();

    // Sources / Imports
    void upsertSource(const QString& id, const QString& kind, const QString& url,
                      const QString& license);
    qint64 createImportRun(const QString& sourceId, const QString& snapshotFilename,
                           const QDate& snapshotDate, const QString& sourceUrl,
                           const QString& license);
    void finishImportRun(qint64 id, int reportCount, int skippedCount);

    // Games
    void upsertGame(const Game& game);
    std::optional<Game> lookupGame(int appId);
    QList<Game> searchGames(const QString& query, int limit = 20);
    int reportCountByAppId(int appId);

    // Reports
    qint64 insertReport(qint64 importRunId, const Report& report, const QString& rawJson);
    void upsertReportSystemInfo(qint64 reportId, const ReportSystemInfo& info);
    QList<ReportRecord> reportsByAppId(int appId);

    // Computed rating from fault flags
    struct GameRating { int total; int yes; int clean; int pctYes() const { return total ? yes*100/total : 0; } int pctClean() const { return total ? clean*100/total : 0; } };
    GameRating gameRating(int appId);

private:
    Database(QSqlDatabase db) : m_db(std::move(db)) {}
    void ensureSchema();
    QSqlDatabase m_db;
    bool m_moved = false;
};

} // namespace ProtonSage
