#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <cstdio>
#include "storage/database.h"
#include "steam/paths.h"
#include "advisor/advisor.h"

#define LOG(fmt, ...) do { fprintf(stderr, fmt "\n", ##__VA_ARGS__); fflush(stderr); } while(0)

int main(int argc, char* argv[]) {
    LOG("S0");
    QCoreApplication app(argc, argv);
    LOG("S1");

    int passed = 0, failed = 0;
    auto check = [&](bool ok, const char* name) {
        if (ok) { passed++; LOG("  PASS: %s", name); }
        else { failed++; LOG("  FAIL: %s", name); }
    };

    LOG("=== ProtonSage Smoke Test ===");

    // 1. Steam paths
    LOG("Test 1: Steam paths");
    auto roots = ProtonSage::existingRoots();
    check(!roots.isEmpty(), "Steam roots found");

    // 2. DB open
    LOG("Test 2: DB open");
    QString dbPath = QDir::homePath() + "/.local/share/protonsage/protonsage.db";
    QFileInfo fi(dbPath);
    check(fi.exists(), "DB file exists");

    auto dbOpt = ProtonSage::Database::open(dbPath);
    check(dbOpt.has_value(), "Database open");
    if (!dbOpt.has_value()) { LOG("ABORT"); return 1; }
    auto& db = *dbOpt;

    // 3. Data status
    LOG("Test 3: Data status");
    auto status = db.status();
    check(status.reportCount > 10000, "Report count > 10000");
    LOG("  actual: reports=%d games=%d", status.reportCount, status.gameCount);
    check(status.gameCount > 1000, "Game count > 1000");

    // 4. Game lookup
    LOG("Test 4: Game lookup");
    auto factorio = db.lookupGame(427520);
    check(factorio.has_value(), "Factorio found");
    auto bl4 = db.lookupGame(1285190);
    check(bl4.has_value(), "Borderlands 4 found");

    // 5. Report counts
    LOG("Test 5: Report counts");
    int fc = db.reportCountByAppId(427520);
    check(fc >= 300, "Factorio report count");
    int bc = db.reportCountByAppId(1285190);
    check(bc >= 280, "Borderlands 4 report count");

    // 6. Reports by AppID
    LOG("Test 6: ReportsByAppId");
    auto records = db.reportsByAppId(427520);
    check(records.size() >= 300, "ReportsByAppId count");

    // 7. Recommendation
    LOG("Test 7: Recommendation");
    ProtonSage::SystemProfile profile;
    profile.gpuVendor = "NVIDIA";
    profile.distro = "CachyOS";
    QList<ProtonSage::Report> reports;
    for (const auto& rec : records) reports.append(rec.report);
    auto rec = ProtonSage::generateRecommendation(
        ProtonSage::Game{427520, "Factorio"}, reports, profile);
    check(!rec.suggestions.isEmpty(), "Suggestions generated");
    check(!rec.summary.isEmpty(), "Summary generated");

    // Freshness
    int fresh = 0, recent = 0;
    for (const auto& rr : rec.rankedReports) {
        if (rr.freshness == "fresh") fresh++;
        else if (rr.freshness == "recent") recent++;
    }
    check(fresh + recent > 0, "Fresh/recent reports exist");

    // 8. Preview
    LOG("Test 8: Preview");
    if (!rec.suggestions.isEmpty()) {
        auto preview = ProtonSage::buildLaunchPreview(rec.suggestions.mid(0, 3));
        check(preview.preview.contains("%command%"), "Preview contains %command%");
    }

    LOG("========== RESULTS ==========");
    LOG("%d passed, %d failed", passed, failed);
    return failed > 0 ? 1 : 0;
}
