#include "models.h"
#include <QJsonArray>
#include <QJsonDocument>

namespace ProtonSage {

QString canonicalSnippet(const QString& s) {
    return s.simplified().toLower();
}

// ── Game ──────────────────────────────────────────────────────────────

QJsonObject Game::toJson() const {
    QJsonObject o;
    o[u"appId"] = appId;
    o[u"name"] = name;
    if (!installPath.isEmpty()) o[u"installPath"] = installPath;
    if (!libraryPath.isEmpty()) o[u"libraryPath"] = libraryPath;
    o[u"launcher"] = launcher;
    if (sizeOnDisk > 0) o[u"sizeOnDisk"] = sizeOnDisk;
    if (stateFlags > 0) o[u"stateFlags"] = stateFlags;
    if (!buildId.isEmpty()) o[u"buildId"] = buildId;
    if (!existingLaunchOptions.isEmpty()) o[u"existingLaunchOptions"] = existingLaunchOptions;
    return o;
}

Game Game::fromJson(const QJsonObject& o) {
    Game g;
    g.appId = o[u"appId"].toInt();
    g.name = o[u"name"].toString();
    g.installPath = o[u"installPath"].toString();
    g.libraryPath = o[u"libraryPath"].toString();
    g.launcher = o["launcher"].toString(QStringLiteral("steam"));
    g.sizeOnDisk = static_cast<qint64>(o[u"sizeOnDisk"].toDouble());
    g.stateFlags = static_cast<qint64>(o[u"stateFlags"].toDouble());
    g.buildId = o[u"buildId"].toString();
    g.existingLaunchOptions = o[u"existingLaunchOptions"].toString();
    return g;
}

// ── SystemProfile ─────────────────────────────────────────────────────

QJsonObject SystemProfile::toJson() const {
    QJsonObject o;
    o["gpuVendor"] = gpuVendor; o["gpuModel"] = gpuModel; o["gpuDriver"] = gpuDriver;
    o["cpu"] = cpu; o["ramGb"] = ramGb; o["distro"] = distro; o["kernel"] = kernel;
    o["sessionType"] = sessionType; o["desktop"] = desktop;
    QJsonObject n;
    n["gpuVendor"] = normGpuVendor; n["gpuModel"] = normGpuModel; n["gpuDriver"] = normGpuDriver;
    n["cpuVendor"] = normCpuVendor; n["cpuClass"] = normCpuClass; n["ramBucket"] = normRamBucket;
    n["distroFamily"] = normDistroFamily; n["kernel"] = normKernel; n["sessionType"] = normSessionType;
    o["normalized"] = n;
    return o;
}

// ── Report ────────────────────────────────────────────────────────────

QJsonObject Report::toJson() const {
    QJsonObject o;
    if (id > 0) o[u"id"] = id;
    if (!sourceReportId.isEmpty()) o[u"sourceReportId"] = sourceReportId;
    o[u"appId"] = appId;
    if (!title.isEmpty()) o[u"title"] = title;
    o[u"timestamp"] = timestamp.toString(Qt::ISODate);
    if (!verdict.isEmpty()) o[u"verdict"] = verdict;
    if (!rating.isEmpty()) o[u"rating"] = rating;
    if (!notes.isEmpty()) o[u"notes"] = notes;
    if (!launchOptions.isEmpty()) o[u"launchOptions"] = launchOptions;
    if (!protonVersion.isEmpty()) o[u"protonVersion"] = protonVersion;
    o[u"sourceId"] = sourceId;
    if (!systemInfo.isEmpty()) {
        QJsonObject si;
        for (auto it = systemInfo.begin(); it != systemInfo.end(); ++it) si[it.key()] = it.value();
        o[u"systemInfo"] = si;
    }
    return o;
}

Report Report::fromJson(const QJsonObject& o) {
    Report r;
    r.id = static_cast<qint64>(o[u"id"].toDouble());
    r.sourceReportId = o[u"sourceReportId"].toString();
    r.appId = o[u"appId"].toInt();
    r.title = o[u"title"].toString();
    r.timestamp = QDateTime::fromString(o[u"timestamp"].toString(), Qt::ISODate);
    r.verdict = o[u"verdict"].toString();
    r.rating = o[u"rating"].toString();
    r.notes = o[u"notes"].toString();
    r.launchOptions = o[u"launchOptions"].toString();
    r.protonVersion = o[u"protonVersion"].toString();
    r.sourceId = o[u"sourceId"].toString();
    const auto si = o[u"systemInfo"].toObject();
    for (auto it = si.begin(); it != si.end(); ++it) r.systemInfo[it.key()] = it.value().toString();
    return r;
}

// ── Simple structs ────────────────────────────────────────────────────

QJsonObject Citation::toJson() const {
    return {{"sourceId", sourceId}, {"reportId", reportId}, {"appId", appId}, {"timestamp", timestamp}, {"snippet", snippet}};
}

QJsonObject SimilarityResult::toJson() const {
    QJsonObject o;
    o[u"score"] = score;
    o[u"matches"] = QJsonArray::fromStringList(matches);
    o[u"mismatches"] = QJsonArray::fromStringList(mismatches);
    o[u"unknowns"] = QJsonArray::fromStringList(unknowns);
    o[u"summary"] = summary;
    return o;
}

QJsonObject RankedReport::toJson() const {
    QJsonObject o;
    o[u"report"] = report.toJson();
    o[u"score"] = score; o[u"recencyScore"] = recencyScore;
    o[u"systemSimilarity"] = systemSimilarity; o[u"qualityScore"] = qualityScore;
    o[u"freshness"] = freshness; o[u"ageDays"] = ageDays;
    o[u"similarity"] = similarity.toJson();
    o[u"reasons"] = QJsonArray::fromStringList(reasons);
    return o;
}

QJsonObject Suggestion::toJson() const {
    QJsonObject o;
    o[u"id"] = id; o[u"snippet"] = snippet; o[u"kind"] = kind;
    QJsonArray srcs;
    for (const auto& c : sources) srcs.append(c.toJson());
    o[u"sources"] = srcs;
    o[u"occurrences"] = occurrences; o[u"recencyScore"] = recencyScore;
    o[u"systemSimilarity"] = systemSimilarity; o[u"confidence"] = confidence;
    if (!conflictNotes.isEmpty()) o[u"conflictNotes"] = QJsonArray::fromStringList(conflictNotes);
    return o;
}

QJsonObject PreviewResult::toJson() const {
    QJsonObject o;
    if (!existing.isEmpty()) o[u"existing"] = existing;
    o[u"preview"] = preview;
    QJsonArray app, skip;
    for (const auto& s : applied) app.append(s.toJson());
    for (const auto& s : skipped) skip.append(s.toJson());
    if (!app.isEmpty()) o[u"applied"] = app;
    if (!skip.isEmpty()) o[u"skipped"] = skip;
    o[u"warnings"] = QJsonArray::fromStringList(warnings);
    o[u"conflicts"] = QJsonArray::fromStringList(conflicts);
    return o;
}

QJsonObject Recommendation::toJson() const {
    QJsonObject o;
    o[u"game"] = game.toJson();
    o[u"summary"] = summary;
    o[u"generatedBy"] = generatedBy;
    QJsonArray sug, cit, rep;
    for (const auto& s : suggestions) sug.append(s.toJson());
    for (const auto& c : citations) cit.append(c.toJson());
    for (const auto& r : rankedReports) rep.append(r.toJson());
    o[u"suggestions"] = sug;
    if (!cit.isEmpty()) o[u"citations"] = cit;
    if (!rep.isEmpty()) o[u"rankedReports"] = rep;
    if (!warnings.isEmpty()) o[u"warnings"] = QJsonArray::fromStringList(warnings);
    return o;
}

QJsonObject ImportRun::toJson() const {
    QJsonObject o;
    o["id"] = id; o["sourceId"] = sourceId;
    o["snapshotFilename"] = snapshotFilename;
    o["snapshotDate"] = snapshotDate.toString(Qt::ISODate);
    o["sourceUrl"] = sourceUrl; o["license"] = license;
    o["startedAt"] = startedAt.toString(Qt::ISODate);
    o["reportCount"] = reportCount; o["skippedCount"] = skippedCount;
    return o;
}

QJsonObject DataStatus::toJson() const {
    QJsonObject o;
    o[u"dbPath"] = dbPath;
    o[u"sourceCount"] = sourceCount; o[u"importRunCount"] = importRunCount;
    o[u"gameCount"] = gameCount; o[u"reportCount"] = reportCount;
    if (latestImport.id > 0) o[u"latestImport"] = latestImport.toJson();
    return o;
}

QJsonObject InstalledGameStatus::toJson() const {
    QJsonObject o;
    o[u"game"] = game.toJson();
    o[u"installed"] = installed; o[u"matchKind"] = matchKind;
    if (dataGame.appId > 0) o[u"dataGame"] = dataGame.toJson();
    if (protonDbAppId > 0) o[u"protonDbAppId"] = protonDbAppId;
    o[u"hasProtonDbReports"] = hasProtonDbReports; o[u"reportCount"] = reportCount;
    return o;
}

QDebug operator<<(QDebug dbg, const Game& g) {
    return dbg.nospace() << "Game(" << g.appId << ", " << g.name << ")";
}

} // namespace ProtonSage
