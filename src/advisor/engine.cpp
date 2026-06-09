#include "advisor.h"
#include "helpers.h"
#include <QCryptographicHash>
#include <QRegularExpression>
#include <algorithm>
#include <set>

namespace ProtonSage {

// ── Hash for suggestion IDs ──────────────────────────────────────────

static QString suggestionId(const QString& kind, const QString& canonical) {
    QByteArray input = (kind + canonical).toUtf8();
    return kind.left(5) + "-" + QCryptographicHash::hash(input, QCryptographicHash::Sha1).toHex().left(8);
}

// ── Extraction ────────────────────────────────────────────────────────

static QList<Suggestion> extractFromReports(const QList<RankedReport>& ranked) {
    struct Candidate {
        QString kind, canonical;
        QSet<QString> sourceKeys;
        double bestSimilarity = 0, bestRecency = 0;
        int occurrences = 0;
    };
    QMap<QPair<QString,QString>, Candidate> candidates;

    // 25 workaround patterns
    static const QList<QPair<QString,QRegularExpression>> patterns = {
        {"workaround", QRegularExpression("\\buse\\s+Proton\\s+Experimental\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\buse\\s+(?:Proton\\s+)?GE[-\\s]?Proton[-\\d]*\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\b(?:switch|change|set)\\s+(?:the\\s+)?(?:compatibility\\s+)?\\s*tool\\s+to\\s+Proton\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\buse\\s+Proton\\s+[\\d.]+\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\bdisabl(?:e|ing)\\s+(?:the\\s+)?intro\\s+videos?\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\bskip\\s+(?:the\\s+)?intro\\s+videos?\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\bskip\\s+(?:the\\s+)?intro\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\bdisabl(?:e|ing)\\s+(?:the\\s+)?launcher\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\b(?:run|start|launch|use|play)\\s+(?:in\\s+)?(?:windowed|window)\\s+mode\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\b(?:force|set)\\s+(?:to\\s+)?(?:windowed|window)\\s+mode\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\bdisabl(?:e|ing)\\s+(?:the\\s+)?fullscreen\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\bdisabl(?:e|ing)\\s+(?:Steam\\s+)?input\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\buse\\s+(?:the\\s+)?native\\s+(?:Linux\\s+)?version\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\b(?:install|use)\\s+(?:protontricks|winetricks)\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\banti[-\\s]?cheat\\b", QRegularExpression::CaseInsensitiveOption)},
        {"workaround", QRegularExpression("\\bEAC\\s+(?:enabled|support)\\b", QRegularExpression::CaseInsensitiveOption)},
        {"diagnostic", QRegularExpression("\\bblack\\s+screen\\b", QRegularExpression::CaseInsensitiveOption)},
        {"diagnostic", QRegularExpression("\\bwhite\\s+screen\\b", QRegularExpression::CaseInsensitiveOption)},
        {"diagnostic", QRegularExpression("\\bscreen\\s+flick(?:er|ering)\\b", QRegularExpression::CaseInsensitiveOption)},
        {"diagnostic", QRegularExpression("\\bno\\s+(?:audio|sound)\\b", QRegularExpression::CaseInsensitiveOption)},
        {"diagnostic", QRegularExpression("\\baudio\\s+(?:crackl|stutter|cut|lag)\\w*\\b", QRegularExpression::CaseInsensitiveOption)},
        {"diagnostic", QRegularExpression("\\bcontroller\\s+(?:not\\s+)?(?:work|detect|respond)\\b", QRegularExpression::CaseInsensitiveOption)},
        {"diagnostic", QRegularExpression("\\b(?:freeze|hang|lock\\s*up)s?\\s+(?:on|at|during|after)?\\b", QRegularExpression::CaseInsensitiveOption)},
        {"diagnostic", QRegularExpression("\\bcrash(?:es)?\\s+(?:on|at|during|after|to|upon|desktop|lobby)\\b", QRegularExpression::CaseInsensitiveOption)},
        {"note", QRegularExpression("\\bno\\s+tweaks?\\s+required\\b", QRegularExpression::CaseInsensitiveOption)},
        {"note", QRegularExpression("\\b(?:works|runs)\\s+(?:out\\s+of\\s+the\\s+box|perfectly|flawlessly)\\b", QRegularExpression::CaseInsensitiveOption)},
    };

    // Known wrapper commands
    static const QStringList wrappers = {"gamemoderun","mangohud","gamescope","prime-run","obs-gamecapture","game-performance","dlss-swapper"};

    for (const auto& rr : ranked) {
        const auto& rep = rr.report;
        QString sourceKey = QString("%1:%2").arg(rep.sourceId, rep.sourceReportId);

        // Check launchOptions field
        if (!rep.launchOptions.isEmpty()) {
            QString snippet = rep.launchOptions;
            QString kind = "launch_option";
            QString canonical = snippet.simplified().toLower();
            auto key = qMakePair(kind, canonical);
            auto& c = candidates[key];
            c.kind = kind; c.canonical = canonical;
            if (!c.sourceKeys.contains(sourceKey)) {
                c.sourceKeys.insert(sourceKey);
                c.occurrences++;
            }
            c.bestSimilarity = qMax(c.bestSimilarity, rr.systemSimilarity);
            c.bestRecency = qMax(c.bestRecency, rr.recencyScore);
        }

        // Scan notes for patterns and %command% lines
        QString searchText = rep.notes + " " + rep.launchOptions;

        // Workaround patterns
        for (const auto& pat : patterns) {
            auto match = pat.second.match(searchText);
            if (match.hasMatch()) {
                QString canonical = match.captured().simplified().toLower();
                auto key = qMakePair(pat.first, canonical);
                auto& c = candidates[key];
                c.kind = pat.first; c.canonical = canonical;
                if (!c.sourceKeys.contains(sourceKey)) {
                    c.sourceKeys.insert(sourceKey);
                    c.occurrences++;
                }
                c.bestSimilarity = qMax(c.bestSimilarity, rr.systemSimilarity);
                c.bestRecency = qMax(c.bestRecency, rr.recencyScore);
            }
        }

        // Extract %command% lines from notes
        static QRegularExpression cmdRe("%command%", QRegularExpression::CaseInsensitiveOption);
        int cmdPos = searchText.indexOf(cmdRe);
        if (cmdPos >= 0) {
            // Grab surrounding text
            int start = qMax(0, cmdPos - 200);
            int end = qMin(searchText.size(), cmdPos + 200);
            QString ctx = searchText.mid(start, end - start).simplified();
            if (ctx.size() > 5) {
                QString canonical = ctx.toLower();
                auto key = qMakePair(QString("launch_option"), canonical);
                auto& c = candidates[key];
                c.kind = "launch_option"; c.canonical = canonical;
                if (!c.sourceKeys.contains(sourceKey)) {
                    c.sourceKeys.insert(sourceKey);
                    c.occurrences++;
                }
                c.bestSimilarity = qMax(c.bestSimilarity, rr.systemSimilarity);
                c.bestRecency = qMax(c.bestRecency, rr.recencyScore);
            }
        }

        // Extract env vars from notes
        static QRegularExpression envRe("\\b(PROTON_|DXVK_|VKD3D_|RADV_|MESA_|WINE|__GL_|NVIDIA_|AMD_|SDL_|WINEDLLOVERRIDES|MANGOHUD|ENABLE_VKBASALT|PULSE_LATENCY_MSEC|DXVK_FRAME_RATE|DXVK_HDR|ENABLE_HDR_WSI|MANGOHUD_CONFIG|LD_PRELOAD)\\w*=\"?[^\"\\s;|&`<>]+\"?",
            QRegularExpression::CaseInsensitiveOption);
        auto envIt = envRe.globalMatch(searchText);
        while (envIt.hasNext()) {
            auto m = envIt.next();
            QString canonical = m.captured().simplified().toLower();
            auto key = qMakePair(QString("env_var"), canonical);
            auto& c = candidates[key];
            c.kind = "env_var"; c.canonical = canonical;
            if (!c.sourceKeys.contains(sourceKey)) {
                c.sourceKeys.insert(sourceKey);
                c.occurrences++;
            }
            c.bestSimilarity = qMax(c.bestSimilarity, rr.systemSimilarity);
            c.bestRecency = qMax(c.bestRecency, rr.recencyScore);
        }

        // Extract wrapper commands from notes
        for (const auto& w : wrappers) {
            if (searchText.contains(w, Qt::CaseInsensitive)) {
                auto key = qMakePair(QString("wrapper"), w.toLower());
                auto& c = candidates[key];
                c.kind = "wrapper"; c.canonical = w.toLower();
                if (!c.sourceKeys.contains(sourceKey)) {
                    c.sourceKeys.insert(sourceKey);
                    c.occurrences++;
                }
                c.bestSimilarity = qMax(c.bestSimilarity, rr.systemSimilarity);
                c.bestRecency = qMax(c.bestRecency, rr.recencyScore);
            }
        }
    }

    // Convert candidates to suggestions
    QList<Suggestion> suggestions;
    for (auto it = candidates.begin(); it != candidates.end(); ++it) {
        const auto& c = it.value();
        Suggestion s;
        s.kind = c.kind;
        s.snippet = c.canonical;
        if (c.occurrences == 0) continue;
        s.id = suggestionId(c.kind, c.canonical);
        s.occurrences = c.occurrences;
        s.systemSimilarity = c.bestSimilarity;
        s.recencyScore = c.bestRecency;

        // Calculate confidence
        double confScore = 0;
        if (c.occurrences >= 7) confScore += 0.4;
        else if (c.occurrences >= 3) confScore += 0.25;
        else confScore += 0.1;
        confScore += c.bestSimilarity * 0.3;
        confScore += c.bestRecency * 0.3;
        if (confScore >= 0.65) s.confidence = "high";
        else if (confScore >= 0.35) s.confidence = "medium";
        else s.confidence = "low";

        suggestions.append(s);
    }

    // Sort: confidence desc, occurrences desc, recency desc, similarity desc
    std::sort(suggestions.begin(), suggestions.end(), [](const Suggestion& a, const Suggestion& b) {
        auto rank = [](const QString& c) { return c == "high" ? 3 : c == "medium" ? 2 : 1; };
        if (rank(a.confidence) != rank(b.confidence)) return rank(a.confidence) > rank(b.confidence);
        if (a.occurrences != b.occurrences) return a.occurrences > b.occurrences;
        if (a.recencyScore != b.recencyScore) return a.recencyScore > b.recencyScore;
        return a.systemSimilarity > b.systemSimilarity;
    });

    return suggestions;
}

QList<Suggestion> extractSuggestions(const QList<RankedReport>& ranked) {
    return extractFromReports(ranked);
}

// ── Recommendation ────────────────────────────────────────────────────

static QString buildSummary(const Game& game, const QList<RankedReport>& ranked,
                             const QList<Suggestion>& suggestions) {
    QString name = game.name.trimmed();
    if (name.isEmpty()) name = QString("appid %1").arg(game.appId);

    if (ranked.isEmpty())
        return QString("No imported ProtonDB reports for %1.").arg(name);

    const auto& top = ranked.first();
    QString date = top.report.timestamp.isValid()
        ? top.report.timestamp.toUTC().toString("yyyy-MM-dd") : "unknown date";

    if (suggestions.isEmpty())
        return QString("No actionable suggestions for %1 from %2 reports.").arg(name).arg(ranked.size());

    const auto& best = suggestions.first();
    QString action = (best.kind == "launch_option" || best.kind == "env_var" || best.kind == "wrapper")
        ? "launch option suggestion" : "suggestion";

    return QString("Start with %1 \"%2\" (%3 confidence). Top evidence is a %4 report from %5 rated %6 with %7 system similarity.")
        .arg(action, best.snippet, best.confidence, top.freshness, date, top.report.rating.isEmpty() ? "?" : top.report.rating)
        .arg(QString::number(static_cast<int>(top.systemSimilarity * 100)) + "%");
}

static QStringList buildWarnings(const QList<RankedReport>& ranked) {
    QStringList w;
    int stale = 0, historical = 0;
    for (const auto& r : ranked) {
        if (r.freshness == "stale") stale++;
        else if (r.freshness == "historical") historical++;
    }
    if (stale > 0 && stale * 3 >= ranked.size())
        w.append(QString("%1 stale reports dominate; data may be outdated.").arg(stale));
    if (historical > 0)
        w.append(QString("%1 reports older than 2 years (historical context only).").arg(historical));
    return w;
}

Recommendation generateRecommendation(const Game& game, const QList<Report>& reports,
                                       const SystemProfile& profile, const QDateTime& now) {
    Recommendation rec;
    rec.game = game;
    rec.generatedBy = "rules";

    auto ranked = rankReports(reports, profile, now);
    auto suggestions = extractSuggestions(ranked);

    rec.rankedReports = ranked;
    rec.suggestions = suggestions;
    rec.summary = buildSummary(game, ranked, suggestions);
    rec.warnings = buildWarnings(ranked);

    // Collect citations from top reports
    int citeLimit = qMin(5, static_cast<int>(ranked.size()));
    for (int i = 0; i < citeLimit; ++i) {
        Citation c;
        c.sourceId = ranked[i].report.sourceId;
        c.reportId = ranked[i].report.sourceReportId;
        c.appId = ranked[i].report.appId;
        c.timestamp = ranked[i].report.timestamp.toString(Qt::ISODate);
        c.snippet = ranked[i].report.notes.left(180);
        rec.citations.append(c);
    }

    return rec;
}

// ── Preview ───────────────────────────────────────────────────────────

PreviewResult buildLaunchPreview(const QList<Suggestion>& selected, const QString& existing) {
    PreviewResult result;
    result.existing = existing.trimmed();

    QStringList prefixTokens, suffixTokens;

    // Parse existing
    if (!result.existing.isEmpty()) {
        QString text = result.existing;
        int cmdIdx = text.toLower().indexOf("%command%");
        if (cmdIdx >= 0) {
            QString before = text.left(cmdIdx).trimmed();
            QString after = text.mid(cmdIdx + 10).trimmed();
            if (!before.isEmpty()) prefixTokens = before.split(' ', Qt::SkipEmptyParts);
            if (!after.isEmpty()) suffixTokens = after.split(' ', Qt::SkipEmptyParts);
        } else {
            suffixTokens.append(result.existing);
            result.warnings.append("Existing options had no %command%; appended as game arguments.");
        }
    }

    // Process selected suggestions
    for (const auto& s : selected) {
        if (!isCopyableKind(s.kind)) {
            result.skipped.append(s);
            continue;
        }
        if (isDangerous(s.snippet)) {
            result.skipped.append(s);
            result.warnings.append(QString("Skipped %1: contains dangerous tokens.").arg(s.id));
            continue;
        }

        QString snippet = s.snippet;
        int cmdIdx = snippet.toLower().indexOf("%command%");
        if (cmdIdx >= 0) {
            QString before = snippet.left(cmdIdx).trimmed();
            QString after = snippet.mid(cmdIdx + 10).trimmed();
            for (const QString& t : before.split(' ', Qt::SkipEmptyParts)) prefixTokens.append(t);
            for (const QString& t : after.split(' ', Qt::SkipEmptyParts)) suffixTokens.append(t);
        } else if (isEnvAssignment(snippet)) {
            prefixTokens.append(snippet);
        } else if (isWrapperCmd(snippet)) {
            prefixTokens.append(snippet);
        } else {
            suffixTokens.append(snippet);
        }
        result.applied.append(s);
    }

    // Deduplicate tokens
    QSet<QString> seen;
    auto dedupe = [&](QStringList& tokens) {
        QStringList out;
        for (const auto& t : tokens) {
            QString canon = trimOuterQuotes(t).toLower();
            if (seen.contains(canon)) continue;
            seen.insert(canon);
            out.append(t);
        }
        tokens = out;
    };
    dedupe(prefixTokens);
    dedupe(suffixTokens);

    // Order prefix: env vars first, wrappers after
    QStringList envs, wrappers;
    for (const auto& t : prefixTokens) {
        if (isEnvAssignment(t)) envs.append(t);
        else wrappers.append(t);
    }

    // Detect env conflicts
    QMap<QString,QSet<QString>> envValues;
    for (const auto& t : envs) {
        auto es = splitEnv(t);
        if (es.ok) envValues[es.name].insert(es.value);
    }
    for (auto it = envValues.begin(); it != envValues.end(); ++it) {
        if (it.value().size() > 1) {
            result.conflicts.append(QString("Conflicting %1: %2").arg(it.key(), QStringList(it.value().begin(), it.value().end()).join(", ")));
        }
    }

    // Build preview string
    QStringList parts;
    parts.append(envs);
    parts.append(wrappers);
    parts.append("%command%");
    parts.append(suffixTokens);
    result.preview = parts.join(' ');

    if (!result.conflicts.isEmpty())
        result.warnings.append("Preview includes conflicting environment assignments.");

    return result;
}

} // namespace ProtonSage
