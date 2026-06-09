package protondb

import (
	"archive/tar"
	"bytes"
	"compress/gzip"
	"context"
	"os"
	"strings"
	"testing"
	"time"

	"protonsage/internal/storage"
)

func TestImportSnapshotRealProtonDBShape(t *testing.T) {
	ctx := context.Background()
	db, err := storage.OpenInMemory()
	if err != nil {
		t.Fatalf("OpenInMemory() error = %v", err)
	}
	defer db.Close()

	payload := `[
		{
			"app": {"steam": {"appId": "352620"}, "title": "Porcunipine"},
			"responses": {
				"notes": {"extra": "Use WINEDLLOVERRIDES=\"dxgi=n,b\" %command%", "verdict": "works with tweak"},
				"protonVersion": "Default",
				"verdict": "yes"
			},
			"timestamp": 1572299227,
			"systemInfo": {
				"cpu": "Intel Core i5-6600K @ 3.50GHz",
				"gpu": "NVIDIA GeForce GTX 980 Ti",
				"gpuDriver": "NVIDIA 396.54",
				"kernel": "4.15.0-33-generic",
				"os": "Ubuntu 18.04.1 LTS",
				"ram": "16 GB"
			}
		}
	]`

	result, err := ImportSnapshot(ctx, db, tarGzReportsFixture(t, payload), SnapshotImportMeta{
		SnapshotFilename: "reports_jun1_2026.tar.gz",
		SourceURL:        "file://reports_jun1_2026.tar.gz",
	})
	if err != nil {
		t.Fatalf("ImportSnapshot() error = %v", err)
	}
	if result.GamesImported != 1 || result.ReportsImported != 1 || result.RecordsSkipped != 0 {
		t.Fatalf("ImportSnapshot() = %+v, want one real-shape report", result)
	}
	game, ok, err := db.LookupGame(ctx, 352620)
	if err != nil {
		t.Fatalf("LookupGame() error = %v", err)
	}
	if !ok || game.Name != "Porcunipine" {
		t.Fatalf("LookupGame() = (%+v, %v), want Porcunipine", game, ok)
	}
	reports, err := db.ReportsByAppID(ctx, 352620)
	if err != nil {
		t.Fatalf("ReportsByAppID() error = %v", err)
	}
	if len(reports) != 1 {
		t.Fatalf("ReportsByAppID() len = %d, want 1", len(reports))
	}
	report := reports[0].Report
	if report.Verdict != "yes" || report.Rating != "yes" || report.ProtonVersion != "Default" {
		t.Fatalf("unexpected report fields: %+v", report)
	}
	if !strings.Contains(report.Notes, `WINEDLLOVERRIDES="dxgi=n,b" %command%`) || !strings.Contains(report.Notes, "works with tweak") {
		t.Fatalf("notes = %q", report.Notes)
	}
	if report.SystemInfo["normalized.gpuVendor"] != "nvidia" || report.SystemInfo["normalized.distroFamily"] != "ubuntu" || report.SystemInfo["normalized.ramBucket"] != "16-31" {
		t.Fatalf("system info = %+v", report.SystemInfo)
	}
}

func TestImportSnapshotFixture(t *testing.T) {
	ctx := context.Background()
	db, err := storage.OpenInMemory()
	if err != nil {
		t.Fatalf("OpenInMemory() error = %v", err)
	}
	defer db.Close()

	fixture, err := os.Open("../../testdata/protondb/reports_sample.tar.gz")
	if err != nil {
		t.Fatalf("open fixture: %v", err)
	}
	defer fixture.Close()

	result, err := ImportSnapshot(ctx, db, fixture, SnapshotImportMeta{
		SnapshotFilename: "reports_sample.tar.gz",
		SnapshotDate:     time.Date(2026, time.June, 1, 0, 0, 0, 0, time.UTC),
		SourceURL:        "file://testdata/protondb/reports_sample.tar.gz",
		LicenseNote:      DataLicenseNote,
	})
	if err != nil {
		t.Fatalf("ImportSnapshot() error = %v", err)
	}
	if result.GamesImported != 2 || result.ReportsImported != 3 || result.SystemInfoImported != 3 || result.RecordsSkipped != 0 {
		t.Fatalf("ImportSnapshot() = %+v, want 2 games, 3 reports, 3 system info, 0 skipped", result)
	}

	status, err := db.Status(ctx)
	if err != nil {
		t.Fatalf("Status() error = %v", err)
	}
	if status.LatestImport == nil {
		t.Fatal("Status().LatestImport is nil")
	}
	if status.LatestImport.SnapshotFilename != "reports_sample.tar.gz" {
		t.Fatalf("snapshot filename = %q", status.LatestImport.SnapshotFilename)
	}
	if !strings.Contains(status.LatestImport.License, "ODbL/DbCL") {
		t.Fatalf("license note = %q, want ODbL/DbCL attribution", status.LatestImport.License)
	}
	if status.LatestImport.SourceURL != "file://testdata/protondb/reports_sample.tar.gz" {
		t.Fatalf("source url = %q", status.LatestImport.SourceURL)
	}
	if status.GameCount != 2 || status.ReportCount != 3 {
		t.Fatalf("Status() = %+v, want 2 games and 3 reports", status)
	}

	game, ok, err := db.LookupGame(ctx, 123)
	if err != nil {
		t.Fatalf("LookupGame() error = %v", err)
	}
	if !ok || game.Name != "Fixture Quest" {
		t.Fatalf("LookupGame(123) = (%+v, %v), want Fixture Quest", game, ok)
	}

	reports, err := db.ReportsByAppID(ctx, 123)
	if err != nil {
		t.Fatalf("ReportsByAppID() error = %v", err)
	}
	if len(reports) != 2 {
		t.Fatalf("ReportsByAppID(123) len = %d, want 2", len(reports))
	}
	if reports[0].SourceReportID != "r-123-new" || reports[1].SourceReportID != "r-123-old" {
		t.Fatalf("report order = %q, %q; want newest-first", reports[0].SourceReportID, reports[1].SourceReportID)
	}
	if reports[0].SystemInfo.GPUVendor != "AMD" || reports[1].SystemInfo.GPUVendor != "NVIDIA" {
		t.Fatalf("GPU vendors = %q/%q, want AMD/NVIDIA", reports[0].SystemInfo.GPUVendor, reports[1].SystemInfo.GPUVendor)
	}
	if reports[0].SystemInfo.Normalized.GPUVendor != "amd" || reports[0].SystemInfo.Normalized.DistroFamily != "fedora" || reports[0].SystemInfo.Normalized.RAMBucket != "32+" {
		t.Fatalf("new report normalized system info = %+v", reports[0].SystemInfo.Normalized)
	}
	if reports[1].SystemInfo.Normalized.GPUVendor != "nvidia" || reports[1].SystemInfo.Normalized.DistroFamily != "arch" {
		t.Fatalf("old report normalized system info = %+v", reports[1].SystemInfo.Normalized)
	}
	if reports[0].Report.SystemInfo["normalized.gpuVendor"] != "amd" {
		t.Fatalf("report system info map = %+v, want normalized.gpuVendor=amd", reports[0].Report.SystemInfo)
	}
	if !strings.Contains(reports[0].Report.LaunchOptions, "%command%") {
		t.Fatalf("launch options = %q, want %%command%%", reports[0].Report.LaunchOptions)
	}

	searchGames, err := db.SearchGames(ctx, "Sample", 10)
	if err != nil {
		t.Fatalf("SearchGames() error = %v", err)
	}
	if len(searchGames) != 1 || searchGames[0].AppID != 456 {
		t.Fatalf("SearchGames(Sample) = %+v, want appid 456", searchGames)
	}
	searchReports, err := db.SearchReports(ctx, "RADV_PERFTEST", 10)
	if err != nil {
		t.Fatalf("SearchReports() error = %v", err)
	}
	if len(searchReports) == 0 || searchReports[0].SourceReportID != "r-123-new" {
		t.Fatalf("SearchReports(RADV_PERFTEST) = %+v, want r-123-new", searchReports)
	}
}

func tarGzReportsFixture(t *testing.T, payload string) *bytes.Reader {
	t.Helper()
	var buf bytes.Buffer
	gz := gzip.NewWriter(&buf)
	tw := tar.NewWriter(gz)
	data := []byte(payload)
	if err := tw.WriteHeader(&tar.Header{Name: reportsJSONName, Mode: 0o644, Size: int64(len(data))}); err != nil {
		t.Fatalf("write tar header: %v", err)
	}
	if _, err := tw.Write(data); err != nil {
		t.Fatalf("write tar payload: %v", err)
	}
	if err := tw.Close(); err != nil {
		t.Fatalf("close tar: %v", err)
	}
	if err := gz.Close(); err != nil {
		t.Fatalf("close gzip: %v", err)
	}
	return bytes.NewReader(buf.Bytes())
}
