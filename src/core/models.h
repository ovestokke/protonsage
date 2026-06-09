#pragma once
#include <QString>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

namespace ProtonSage {

// ── Enums / Constants ───────────────────────────────────────────────

namespace LaunchOptionKind { inline const QString LaunchOption = QStringLiteral("launch_option"); inline const QString EnvVar = QStringLiteral("env_var"); inline const QString Wrapper = QStringLiteral("wrapper"); inline const QString Workaround = QStringLiteral("workaround"); inline const QString Diagnostic = QStringLiteral("diagnostic"); inline const QString Note = QStringLiteral("note"); }
namespace Confidence { inline const QString High = QStringLiteral("high"); inline const QString Medium = QStringLiteral("medium"); inline const QString Low = QStringLiteral("low"); }
namespace Freshness { inline const QString Fresh = QStringLiteral("fresh"); inline const QString Recent = QStringLiteral("recent"); inline const QString Stale = QStringLiteral("stale"); inline const QString Historical = QStringLiteral("historical"); }
namespace RecommendationSource { inline const QString Rules = QStringLiteral("rules"); inline const QString AI = QStringLiteral("ai"); }

// ── Models ───────────────────────────────────────────────────────────

struct Game {
    int appId = 0;
    QString name;
    QString installPath;
    QString libraryPath;
    QString launcher = QStringLiteral("steam");
    qint64 sizeOnDisk = 0;
    qint64 stateFlags = 0;
    QString buildId;
    QString existingLaunchOptions;

    QJsonObject toJson() const;
    static Game fromJson(const QJsonObject& o);
};

struct SystemProfile {
    QString gpuVendor, gpuModel, gpuDriver;
    QString cpu;
    double ramGb = 0;
    QString distro, kernel, sessionType, desktop;

    // normalized comparison categories
    QString normGpuVendor, normGpuModel, normGpuDriver;
    QString normCpuVendor, normCpuClass;
    QString normRamBucket;
    QString normDistroFamily;
    QString normKernel, normSessionType;

    QMap<QString, QString> raw;

    QJsonObject toJson() const;
    static SystemProfile fromJson(const QJsonObject& o);
};

struct Report {
    qint64 id = 0;
    QString sourceReportId;
    int appId = 0;
    QString title;
    QDateTime timestamp;
    QString verdict;
    QString rating;
    QString notes;
    QString launchOptions;
    QString protonVersion;
    QMap<QString, QString> systemInfo;
    QString sourceId;

    QJsonObject toJson() const;
    static Report fromJson(const QJsonObject& o);
};

struct Citation {
    QString sourceId, reportId;
    int appId = 0;
    QString timestamp, snippet;

    QJsonObject toJson() const;
};

struct SimilarityResult {
    double score = 0;
    QStringList matches, mismatches, unknowns;
    QString summary;

    QJsonObject toJson() const;
};

struct RankedReport {
    Report report;
    double score = 0, recencyScore = 0, systemSimilarity = 0, qualityScore = 0;
    QString freshness;
    int ageDays = 0;
    SimilarityResult similarity;
    QStringList reasons;

    QJsonObject toJson() const;
};

struct Suggestion {
    QString id, snippet, kind;
    QString label;       // human-readable name
    QString description;  // what it does, when to use
    QString category;     // "Recommended", "Performance", "Compatibility", "Debug"
    QList<Citation> sources;
    int occurrences = 0;
    double recencyScore = 0, systemSimilarity = 0;
    QString confidence;
    QStringList conflictNotes;

    QJsonObject toJson() const;
};

struct PreviewResult {
    QString existing, preview;
    QList<Suggestion> applied, skipped;
    QStringList warnings, conflicts;

    QJsonObject toJson() const;
};

struct Recommendation {
    Game game;
    QString summary;
    QList<Suggestion> suggestions;
    QList<Citation> citations;
    QList<RankedReport> rankedReports;
    QStringList warnings;
    QString generatedBy = RecommendationSource::Rules;

    QJsonObject toJson() const;
};

struct ImportRun {
    qint64 id = 0;
    QString sourceId, snapshotFilename;
    QDate snapshotDate;
    QString sourceUrl, license;
    QDateTime startedAt, finishedAt;
    int reportCount = 0, skippedCount = 0;

    QJsonObject toJson() const;
};

struct DataStatus {
    QString dbPath;
    int sourceCount = 0, importRunCount = 0, gameCount = 0, reportCount = 0;
    ImportRun latestImport;

    QJsonObject toJson() const;
};

struct InstalledGameStatus {
    Game game;
    bool installed = false;
    QString matchKind;
    Game dataGame;
    int protonDbAppId = 0;
    bool hasProtonDbReports = false;
    int reportCount = 0;

    QJsonObject toJson() const;
};

// ── Utility helpers ──────────────────────────────────────────────────

QString canonicalSnippet(const QString& s);
QDebug operator<<(QDebug dbg, const Game& g);

} // namespace ProtonSage
