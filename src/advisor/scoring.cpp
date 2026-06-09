#include "advisor.h"
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace ProtonSage {

// ── Internal helpers ─────────────────────────────────────────────────

namespace {

constexpr int kFreshDays      = 90;
constexpr int kRecentDays     = 365;
constexpr int kHistoricalDays = 730;

double roundScore(double value) {
    return std::round(value * 1000.0) / 1000.0;
}

double clamp(double value, double min, double max) {
    return std::clamp(value, min, max);
}

// Normalize a comparable value: lowercase, trimmed, "unknown" → empty.
QString normalizeComparableValue(const QString& value) {
    QString v = value.trimmed().toLower();
    if (v.isEmpty() || v == QStringLiteral("unknown"))
        return {};
    return v;
}

// Score for a single field comparison (exact or partial match).
double fieldMatchScore(const QString& user, const QString& report, bool partial) {
    if (user == report)
        return 1.0;
    if (partial && user.size() >= 4 && report.size() >= 4 &&
        (user.contains(report) || report.contains(user)))
        return 0.7;
    return 0.0;
}

QString unknownFieldExplanation(const QString& name, const QString& userValue, const QString& reportValue) {
    if (userValue.isEmpty() && reportValue.isEmpty())
        return name + QStringLiteral(" unknown for user and report");
    if (userValue.isEmpty())
        return name + QStringLiteral(" unknown for user");
    return name + QStringLiteral(" unknown for report");
}

QString similaritySummary(const SimilarityResult& result) {
    QStringList parts;

    auto limit = [](const QStringList& values, int n) -> QStringList {
        if (values.size() <= n) return values;
        QStringList out = values.mid(0, n);
        out << QString::number(values.size() - n) + QStringLiteral(" more");
        return out;
    };

    if (!result.matches.isEmpty())
        parts << QStringLiteral("matches ") + limit(result.matches, 3).join(QStringLiteral(", "));
    if (!result.mismatches.isEmpty())
        parts << QStringLiteral("differs on ") + limit(result.mismatches, 3).join(QStringLiteral(", "));
    if (!result.unknowns.isEmpty())
        parts << QStringLiteral("unknown ") + limit(result.unknowns, 2).join(QStringLiteral(", "));

    if (parts.isEmpty())
        return QStringLiteral("No comparable system fields were available.");

    return parts.join(QStringLiteral("; ")) + QStringLiteral(".");
}

// Build a SystemProfile from a Report's systemInfo map,
// preferring keys with "normalized." prefix (case-insensitive fallback).
SystemProfile normalizedReportProfile(const Report& report) {
    SystemProfile profile;

    // Helper: try exact keys first, then case-insensitive.
    auto apply = [&](QString& target, const QStringList& keys) {
        for (const auto& key : keys) {
            auto it = report.systemInfo.constFind(key);
            if (it != report.systemInfo.constEnd()) {
                QString v = it.value().trimmed().toLower();
                if (!v.isEmpty()) { target = v; return; }
            }
        }
        // Case-insensitive fallback
        for (const auto& key : keys) {
            for (auto it = report.systemInfo.constBegin(); it != report.systemInfo.constEnd(); ++it) {
                if (it.key().compare(key, Qt::CaseInsensitive) == 0) {
                    QString v = it.value().trimmed().toLower();
                    if (!v.isEmpty()) { target = v; return; }
                }
            }
        }
    };

    apply(profile.normGpuVendor,    QStringList{QStringLiteral("normalized.gpuVendor")});
    apply(profile.normGpuModel,     QStringList{QStringLiteral("normalized.gpuModel")});
    apply(profile.normGpuDriver,    QStringList{QStringLiteral("normalized.gpuDriver")});
    apply(profile.normCpuVendor,    QStringList{QStringLiteral("normalized.cpuVendor")});
    apply(profile.normCpuClass,     QStringList{QStringLiteral("normalized.cpuClass")});
    apply(profile.normRamBucket,    QStringList{QStringLiteral("normalized.ramBucket")});
    apply(profile.normDistroFamily, QStringList{QStringLiteral("normalized.distroFamily")});
    apply(profile.normKernel,       QStringList{QStringLiteral("normalized.kernel")});
    apply(profile.normSessionType,  QStringList{QStringLiteral("normalized.sessionType")});

    return profile;
}

bool normalizedProfileIsZero(const SystemProfile& p) {
    return p.normGpuVendor.isEmpty()  && p.normGpuModel.isEmpty() && p.normGpuDriver.isEmpty()
        && p.normCpuVendor.isEmpty()  && p.normCpuClass.isEmpty()
        && p.normRamBucket.isEmpty()
        && p.normDistroFamily.isEmpty()
        && p.normKernel.isEmpty()
        && p.normSessionType.isEmpty();
}

QString reportStableID(const Report& report) {
    if (!report.sourceReportId.isEmpty())
        return report.sourceReportId;
    if (report.id != 0)
        return QString::number(report.id);
    return QString::number(report.appId) + QStringLiteral(":") + report.timestamp.toUTC().toString(Qt::ISODate);
}

struct SimilarityField {
    QString name;
    QString user;
    QString report;
    double weight = 0;
    bool partial = false;
};

} // anonymous namespace

// ── Public API ───────────────────────────────────────────────────────

double recencyScore(const QDateTime& reportTime, const QDateTime& now) {
    if (!reportTime.isValid())
        return 0.0;
    QDateTime n = now.isValid() ? now : QDateTime::currentDateTimeUtc();
    double ageDays = reportTime.msecsTo(n) / 86400000.0;
    if (ageDays <= 0.0)
        return 1.0;

    if (ageDays <= 30.0)
        return 1.0;
    if (ageDays <= kFreshDays)
        return roundScore(0.95 - ((ageDays - 30.0) / (kFreshDays - 30.0)) * 0.10);
    if (ageDays < kRecentDays)
        return roundScore(0.75 - ((ageDays - kFreshDays) / (kRecentDays - kFreshDays)) * 0.35);
    if (ageDays < kHistoricalDays)
        return roundScore(0.24 - ((ageDays - kRecentDays) / (kHistoricalDays - kRecentDays)) * 0.12);
    return 0.05;
}

QString freshnessLabel(const QDateTime& reportTime, const QDateTime& now) {
    int age = reportAgeDays(reportTime, now);
    if (age < kFreshDays)     return Freshness::Fresh;
    if (age < kRecentDays)    return Freshness::Recent;
    if (age < kHistoricalDays) return Freshness::Stale;
    return Freshness::Historical;
}

int reportAgeDays(const QDateTime& reportTime, const QDateTime& now) {
    if (!reportTime.isValid()) return 0;
    QDateTime n = now.isValid() ? now : QDateTime::currentDateTimeUtc();
    int age = static_cast<int>(reportTime.msecsTo(n) / 86400000.0);
    return std::max(0, age);
}

SimilarityResult systemSimilarity(const SystemProfile& user, const SystemProfile& report) {
    QList<SimilarityField> fields = {
        {QStringLiteral("GPU vendor"),    user.normGpuVendor,    report.normGpuVendor,    0.28, false},
        {QStringLiteral("GPU model"),     user.normGpuModel,     report.normGpuModel,     0.12, true},
        {QStringLiteral("GPU driver"),    user.normGpuDriver,    report.normGpuDriver,    0.10, false},
        {QStringLiteral("distro family"), user.normDistroFamily, report.normDistroFamily, 0.15, false},
        {QStringLiteral("kernel"),        user.normKernel,       report.normKernel,       0.10, false},
        {QStringLiteral("RAM bucket"),    user.normRamBucket,    report.normRamBucket,    0.08, false},
        {QStringLiteral("session type"),  user.normSessionType,  report.normSessionType,  0.07, false},
        {QStringLiteral("CPU vendor"),    user.normCpuVendor,     report.normCpuVendor,    0.05, false},
        {QStringLiteral("CPU class"),     user.normCpuClass,      report.normCpuClass,     0.05, false},
    };

    double matchedWeight = 0.0;
    double comparableWeight = 0.0;
    SimilarityResult result;

    for (const auto& field : fields) {
        QString userValue   = normalizeComparableValue(field.user);
        QString reportValue = normalizeComparableValue(field.report);

        if (userValue.isEmpty() || reportValue.isEmpty()) {
            result.unknowns.append(unknownFieldExplanation(field.name, userValue, reportValue));
            continue;
        }

        comparableWeight += field.weight;
        double matchScore = fieldMatchScore(userValue, reportValue, field.partial);
        if (matchScore > 0.0) {
            matchedWeight += field.weight * matchScore;
            result.matches.append(field.name.toLower() + QStringLiteral(" ") + reportValue);
        } else {
            result.mismatches.append(field.name.toLower()
                                     + QStringLiteral(" user=") + userValue
                                     + QStringLiteral(" report=") + reportValue);
        }
    }

    if (comparableWeight == 0.0) {
        result.score = 0.5;
        result.summary = QStringLiteral("No comparable system fields were available.");
        return result;
    }

    result.score = roundScore(matchedWeight / comparableWeight);
    result.summary = similaritySummary(result);
    return result;
}

// ── ReportQualityScore ──────────────────────────────────────────────

namespace {

double reportQualityScore(const Report& report) {
    double score = 0.45;
    QString rating = report.rating.trimmed().toLower();

    if (rating == QStringLiteral("platinum"))  score = 0.95;
    else if (rating == QStringLiteral("gold"))   score = 0.85;
    else if (rating == QStringLiteral("silver")) score = 0.65;
    else if (rating == QStringLiteral("bronze")) score = 0.45;
    else if (rating == QStringLiteral("borked")) score = 0.20;

    // Boost for concrete tweak signals
    if (!report.launchOptions.trimmed().isEmpty())
        score += 0.08;
    else if (report.notes.contains(QStringLiteral("%command%"), Qt::CaseInsensitive))
        score += 0.08;
    else {
        // Check for env assignments or wrapper mentions in notes
        static const QRegularExpression envRe(QStringLiteral("\\b[A-Z_][A-Z0-9_]{2,}="));
        static const QRegularExpression wrapperRe(
            QStringLiteral("\\b(gamemoderun|mangohud|gamescope|prime-run|obs-gamecapture)\\b"),
            QRegularExpression::CaseInsensitiveOption);
        if (report.notes.indexOf(envRe) >= 0 || report.notes.indexOf(wrapperRe) >= 0)
            score += 0.08;
    }

    if (!report.verdict.trimmed().isEmpty())
        score += 0.03;

    return roundScore(clamp(score, 0.0, 1.0));
}

} // namespace

// ── RankReports ─────────────────────────────────────────────────────

QList<RankedReport> rankReports(const QList<Report>& reports,
                                 const SystemProfile& profile,
                                 const QDateTime& now) {
    QDateTime n = now.isValid() ? now : QDateTime::currentDateTimeUtc();
    SystemProfile userProfile = profile;
    Q_UNUSED(userProfile);

    QList<RankedReport> ranked;
    ranked.reserve(reports.size());

    for (const auto& report : reports) {
        double recency   = recencyScore(report.timestamp, n);
        SimilarityResult sim = systemSimilarity(profile, normalizedReportProfile(report));
        double quality   = reportQualityScore(report);
        QString freshness = freshnessLabel(report.timestamp, n);
        int ageDays      = reportAgeDays(report.timestamp, n);

        RankedReport item;
        item.report           = report;
        item.score            = roundScore(recency * 0.72 + sim.score * 0.20 + quality * 0.08);
        item.recencyScore     = recency;
        item.systemSimilarity = sim.score;
        item.qualityScore     = quality;
        item.freshness         = freshness;
        item.ageDays           = ageDays;
        item.similarity        = sim;

        // Build reasons
        QStringList reasons;
        reasons << QString(QStringLiteral("%1 report (%2 days old, recency %3"))
                       .arg(freshness).arg(ageDays).arg(recency, 0, 'f', 3);
        reasons << QString(QStringLiteral("system similarity %1: %2"))
                       .arg(sim.score, 0, 'f', 3).arg(sim.summary);
        reasons << QString(QStringLiteral("quality %1 from rating/verdict/concrete tweak signals"))
                       .arg(quality, 0, 'f', 3);

        if (!report.launchOptions.trimmed().isEmpty())
            reasons << QStringLiteral("explicit launch options field present");

        if (freshness == Freshness::Stale || freshness == Freshness::Historical)
            reasons << QStringLiteral("treat as historical context, not current truth");

        item.reasons = reasons;
        ranked.append(item);
    }

    // Sort: descending score, then descending timestamp, then stable ID
    std::sort(ranked.begin(), ranked.end(),
              [](const RankedReport& a, const RankedReport& b) {
                  if (qFuzzyCompare(a.score, b.score)) {
                      if (a.report.timestamp == b.report.timestamp) {
                          return reportStableID(a.report) < reportStableID(b.report);
                      }
                      return a.report.timestamp > b.report.timestamp;
                  }
                  return a.score > b.score;
              });

    return ranked;
}

} // namespace ProtonSage
