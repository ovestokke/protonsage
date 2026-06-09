package storage

import (
	"context"
	"strings"
	"testing"
	"time"

	"protonsage/internal/core"
)

func TestStorageInsertLookupAndSearch(t *testing.T) {
	ctx := context.Background()
	db, err := OpenInMemory()
	if err != nil {
		t.Fatalf("OpenInMemory() error = %v", err)
	}
	defer db.Close()

	importDate := time.Date(2026, time.June, 1, 0, 0, 0, 0, time.UTC)
	if err := db.UpsertSource(ctx, SourceInput{
		ID:         "protondb-data:test",
		Kind:       "protondb-data",
		URL:        "file://fixture",
		License:    "ODbL/DbCL test attribution",
		ImportedAt: importDate,
	}); err != nil {
		t.Fatalf("UpsertSource() error = %v", err)
	}
	runID, err := db.CreateImportRun(ctx, ImportRunInput{
		SourceID:         "protondb-data:test",
		SnapshotFilename: "reports_sample.tar.gz",
		SnapshotDate:     importDate,
		SourceURL:        "file://fixture",
		License:          "ODbL/DbCL test attribution",
		StartedAt:        importDate,
	})
	if err != nil {
		t.Fatalf("CreateImportRun() error = %v", err)
	}
	if err := db.UpsertGame(ctx, core.Game{AppID: 123, Name: "Fixture Quest", Launcher: core.LauncherSteam}); err != nil {
		t.Fatalf("UpsertGame() error = %v", err)
	}

	oldReportID, err := db.InsertReport(ctx, ReportInput{
		ImportRunID:    runID,
		SourceReportID: "old",
		RawJSON:        `{"id":"old"}`,
		Report: core.Report{
			AppID:         123,
			Title:         "Fixture Quest",
			Timestamp:     time.Date(2024, time.January, 15, 10, 0, 0, 0, time.UTC),
			Rating:        "silver",
			Notes:         "Use PROTON_USE_WINED3D=1 %command% on older NVIDIA drivers.",
			LaunchOptions: "PROTON_USE_WINED3D=1 %command%",
			SourceID:      "protondb-data:test",
		},
	})
	if err != nil {
		t.Fatalf("InsertReport(old) error = %v", err)
	}
	if err := db.UpsertReportSystemInfo(ctx, oldReportID, ReportSystemInfo{GPUVendor: "NVIDIA", GPUModel: "GeForce GTX 1080", GPUDriver: "535.154", RawJSON: `{"gpuVendor":"NVIDIA"}`}); err != nil {
		t.Fatalf("UpsertReportSystemInfo() error = %v", err)
	}

	if _, err := db.InsertReport(ctx, ReportInput{
		ImportRunID:    runID,
		SourceReportID: "new",
		RawJSON:        `{"id":"new"}`,
		Report: core.Report{
			AppID:         123,
			Title:         "Fixture Quest",
			Timestamp:     time.Date(2026, time.May, 20, 18, 30, 0, 0, time.UTC),
			Rating:        "gold",
			Notes:         "Set RADV_PERFTEST=gpl %command% on Mesa.",
			LaunchOptions: "RADV_PERFTEST=gpl %command%",
			SourceID:      "protondb-data:test",
		},
	}); err != nil {
		t.Fatalf("InsertReport(new) error = %v", err)
	}
	if err := db.FinishImportRun(ctx, runID, 2, 0); err != nil {
		t.Fatalf("FinishImportRun() error = %v", err)
	}

	game, ok, err := db.LookupGame(ctx, 123)
	if err != nil {
		t.Fatalf("LookupGame() error = %v", err)
	}
	if !ok || game.Name != "Fixture Quest" {
		t.Fatalf("LookupGame() = (%+v, %v), want Fixture Quest", game, ok)
	}

	reports, err := db.ReportsByAppID(ctx, 123)
	if err != nil {
		t.Fatalf("ReportsByAppID() error = %v", err)
	}
	if len(reports) != 2 {
		t.Fatalf("ReportsByAppID() len = %d, want 2", len(reports))
	}
	if reports[0].SourceReportID != "new" || reports[1].SourceReportID != "old" {
		t.Fatalf("reports order = %s, %s; want newest-first", reports[0].SourceReportID, reports[1].SourceReportID)
	}
	if reports[1].SystemInfo.GPUVendor != "NVIDIA" {
		t.Fatalf("system info GPUVendor = %q, want NVIDIA", reports[1].SystemInfo.GPUVendor)
	}
	if reports[1].SystemInfo.Normalized.GPUVendor != "nvidia" || reports[1].Report.SystemInfo["normalized.gpuVendor"] != "nvidia" {
		t.Fatalf("normalized system info = %+v / %+v, want nvidia", reports[1].SystemInfo.Normalized, reports[1].Report.SystemInfo)
	}
	count, err := db.ReportCountByAppID(ctx, 123)
	if err != nil {
		t.Fatalf("ReportCountByAppID() error = %v", err)
	}
	if count != 2 {
		t.Fatalf("ReportCountByAppID() = %d, want 2", count)
	}

	gameResults, err := db.SearchGames(ctx, "Fixture", 10)
	if err != nil {
		t.Fatalf("SearchGames() error = %v", err)
	}
	if len(gameResults) != 1 || gameResults[0].AppID != 123 {
		t.Fatalf("SearchGames() = %+v, want appid 123", gameResults)
	}
	reportResults, err := db.SearchReports(ctx, "RADV_PERFTEST", 10)
	if err != nil {
		t.Fatalf("SearchReports() error = %v", err)
	}
	if len(reportResults) == 0 || !strings.Contains(reportResults[0].Report.LaunchOptions, "RADV_PERFTEST") {
		t.Fatalf("SearchReports() = %+v, want RADV_PERFTEST report", reportResults)
	}

	status, err := db.Status(ctx)
	if err != nil {
		t.Fatalf("Status() error = %v", err)
	}
	if status.SourceCount != 1 || status.ImportRunCount != 1 || status.GameCount != 1 || status.ReportCount != 2 {
		t.Fatalf("Status() = %+v, want 1 source/run/game and 2 reports", status)
	}
	if status.LatestImport == nil || status.LatestImport.SnapshotFilename != "reports_sample.tar.gz" {
		t.Fatalf("LatestImport = %+v, want reports_sample.tar.gz", status.LatestImport)
	}
}
