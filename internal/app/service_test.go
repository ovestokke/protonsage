package app

import (
	"context"
	"os"
	"path/filepath"
	"strconv"
	"testing"
	"time"

	"protonsage/internal/core"
	"protonsage/internal/protondb"
	"protonsage/internal/storage"
)

func TestGetInstalledGamesMatchesImportedData(t *testing.T) {
	ctx := context.Background()
	dbPath := filepath.Join(t.TempDir(), "protonsage.db")
	importFixtureDB(t, ctx, dbPath)

	root := t.TempDir()
	steamapps := filepath.Join(root, "steamapps")
	if err := os.MkdirAll(steamapps, 0o755); err != nil {
		t.Fatal(err)
	}
	copyFixture(t, "../../testdata/steam/appmanifest_123.acf", filepath.Join(steamapps, "appmanifest_123.acf"))
	writeManifest(t, filepath.Join(steamapps, "appmanifest_999.acf"), 999, "Sample Tactics", "Sample Tactics")

	statuses, err := NewService().GetInstalledGames(ctx, dbPath, root)
	if err != nil {
		t.Fatal(err)
	}
	if len(statuses) != 2 {
		t.Fatalf("len(statuses) = %d", len(statuses))
	}
	byAppID := map[int]InstalledGameStatus{}
	for _, status := range statuses {
		byAppID[status.Game.AppID] = status
	}
	if byAppID[123].MatchKind != "appid" || !byAppID[123].HasProtonDBReports || byAppID[123].ReportCount != 2 {
		t.Fatalf("app 123 status = %+v", byAppID[123])
	}
	if byAppID[999].MatchKind != "name" || byAppID[999].ProtonDBAppID != 456 || !byAppID[999].HasProtonDBReports || byAppID[999].ReportCount != 1 {
		t.Fatalf("app 999 name fallback status = %+v", byAppID[999])
	}
}

func TestGetRecommendationAndPreview(t *testing.T) {
	ctx := context.Background()
	dbPath := filepath.Join(t.TempDir(), "protonsage.db")
	importFixtureDB(t, ctx, dbPath)

	profile := core.SystemProfile{Normalized: core.NormalizedSystemProfile{
		GPUVendor:    "amd",
		GPUModel:     "radeon rx 7800 xt",
		DistroFamily: "fedora",
		Kernel:       "6.14",
		RAMBucket:    "32+",
	}}
	recommendation, err := NewService().getRecommendationForProfile(ctx, dbPath, 123, profile, time.Date(2026, time.June, 1, 0, 0, 0, 0, time.UTC))
	if err != nil {
		t.Fatal(err)
	}
	if len(recommendation.RankedReports) != 2 || recommendation.RankedReports[0].Report.SourceReportID != "r-123-new" {
		t.Fatalf("ranked reports = %+v", recommendation.RankedReports)
	}
	if len(recommendation.Suggestions) == 0 || len(recommendation.Citations) == 0 {
		t.Fatalf("recommendation = %+v, want suggestions and citations", recommendation)
	}

	preview := NewService().BuildLaunchPreview(ctx, recommendation.Suggestions[:1], "")
	if preview.Preview == "" || preview.Preview == "%command%" {
		t.Fatalf("preview = %+v, want selected launch option included", preview)
	}
}

func importFixtureDB(t *testing.T, ctx context.Context, dbPath string) {
	t.Helper()
	db, err := storage.Open(dbPath)
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	fixture, err := os.Open("../../testdata/protondb/reports_sample.tar.gz")
	if err != nil {
		t.Fatal(err)
	}
	defer fixture.Close()
	_, err = protondb.ImportSnapshot(ctx, db, fixture, protondb.SnapshotImportMeta{
		SnapshotFilename: "reports_sample.tar.gz",
		SnapshotDate:     time.Date(2026, time.June, 1, 0, 0, 0, 0, time.UTC),
		SourceURL:        "file://testdata/protondb/reports_sample.tar.gz",
		LicenseNote:      protondb.DataLicenseNote,
	})
	if err != nil {
		t.Fatal(err)
	}
}

func copyFixture(t *testing.T, src, dst string) {
	t.Helper()
	data, err := os.ReadFile(src)
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(dst, data, 0o644); err != nil {
		t.Fatal(err)
	}
}

func writeManifest(t *testing.T, path string, appid int, name string, installDir string) {
	t.Helper()
	manifest := `"AppState"
{
    "appid" "` + strconv.Itoa(appid) + `"
    "name" "` + name + `"
    "StateFlags" "4"
    "installdir" "` + installDir + `"
}
`
	if err := os.WriteFile(path, []byte(manifest), 0o644); err != nil {
		t.Fatal(err)
	}
}
