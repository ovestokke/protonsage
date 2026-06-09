package protondb

import (
	"testing"
	"time"
)

func TestParseSnapshotFilename(t *testing.T) {
	date, ok := ParseSnapshotFilename("reports_jun1_2026.tar.gz")
	if !ok {
		t.Fatal("expected valid snapshot filename")
	}
	want := time.Date(2026, time.June, 1, 0, 0, 0, 0, time.UTC)
	if !date.Equal(want) {
		t.Fatalf("date = %s, want %s", date, want)
	}
}

func TestParseSnapshotFilenameRejectsInvalid(t *testing.T) {
	invalid := []string{
		"reports_jun32_2026.tar.gz",
		"reports_foo1_2026.tar.gz",
		"reports_jun1_2026.zip",
		"README.md",
	}
	for _, filename := range invalid {
		if _, ok := ParseSnapshotFilename(filename); ok {
			t.Fatalf("%s parsed as valid", filename)
		}
	}
}

func TestSelectLatestSnapshot(t *testing.T) {
	snapshot, ok := SelectLatestSnapshot([]string{
		"README.md",
		"reports_may1_2026.tar.gz",
		"reports_jun1_2026.tar.gz",
		"reports_dec31_2025.tar.gz",
	})
	if !ok {
		t.Fatal("expected latest snapshot")
	}
	if snapshot.Filename != "reports_jun1_2026.tar.gz" {
		t.Fatalf("filename = %q", snapshot.Filename)
	}
	if snapshot.URL == "" {
		t.Fatal("expected default raw URL")
	}
}
