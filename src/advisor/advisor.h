#pragma once
#include <QDateTime>
#include <QList>
#include <QString>
#include "core/models.h"

namespace ProtonSage {

// ── Suggestion metadata ──────────────────────────────────────────────

struct SuggestionMeta { QString label, desc, category; };
SuggestionMeta suggestionMeta(const QString& snippet);

// ── Scoring ──────────────────────────────────────────────────────────

double recencyScore(const QDateTime& reportTime, const QDateTime& now = {});
QString freshnessLabel(const QDateTime& reportTime, const QDateTime& now = {});
int reportAgeDays(const QDateTime& reportTime, const QDateTime& now = {});
SimilarityResult systemSimilarity(const SystemProfile& user, const SystemProfile& report);
QList<RankedReport> rankReports(const QList<Report>& reports, const SystemProfile& profile, const QDateTime& now = {});

// ── Extraction ────────────────────────────────────────────────────────

QList<Suggestion> extractSuggestions(const QList<RankedReport>& ranked);

// ── Recommendation ────────────────────────────────────────────────────

Recommendation generateRecommendation(const Game& game, const QList<Report>& reports,
                                       const SystemProfile& profile, const QDateTime& now = {});

// ── Preview ───────────────────────────────────────────────────────────

PreviewResult buildLaunchPreview(const QList<Suggestion>& selected, const QString& existing = {});

} // namespace ProtonSage
