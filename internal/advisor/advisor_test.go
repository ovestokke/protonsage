package advisor

import (
	"strings"
	"testing"
	"time"

	"protonsage/internal/core"
)

func TestRecencyScoreAndFreshnessLabels(t *testing.T) {
	now := time.Date(2026, time.June, 1, 0, 0, 0, 0, time.UTC)
	fresh := time.Date(2026, time.May, 20, 0, 0, 0, 0, time.UTC)
	recent := time.Date(2025, time.December, 1, 0, 0, 0, 0, time.UTC)
	stale := time.Date(2025, time.May, 1, 0, 0, 0, 0, time.UTC)
	historical := time.Date(2023, time.May, 1, 0, 0, 0, 0, time.UTC)

	if score := RecencyScore(fresh, now); score < 0.95 {
		t.Fatalf("fresh recency = %.3f, want high", score)
	}
	if score := RecencyScore(stale, now); score >= 0.25 {
		t.Fatalf("stale recency = %.3f, want sharp drop after 1 year", score)
	}
	if score := RecencyScore(historical, now); score != 0.05 {
		t.Fatalf("historical recency = %.3f, want 0.05", score)
	}
	cases := []struct {
		name string
		time time.Time
		want string
	}{
		{"fresh", fresh, core.FreshnessFresh},
		{"recent", recent, core.FreshnessRecent},
		{"stale", stale, core.FreshnessStale},
		{"historical", historical, core.FreshnessHistorical},
	}
	for _, tc := range cases {
		if got := FreshnessLabel(tc.time, now); got != tc.want {
			t.Fatalf("%s label = %q, want %q", tc.name, got, tc.want)
		}
	}
}

func TestSystemSimilarityExactVendorAndMismatch(t *testing.T) {
	user := core.NormalizedSystemProfile{
		GPUVendor:    "nvidia",
		GPUModel:     "geforce rtx 4070 super",
		GPUDriver:    "550.90",
		DistroFamily: "arch",
		Kernel:       "6.14",
		RAMBucket:    "32+",
		SessionType:  "wayland",
		CPUVendor:    "amd",
		CPUClass:     "ryzen 7",
	}
	exact := user
	match := SystemSimilarity(user, exact)
	if match.Score != 1 {
		t.Fatalf("exact similarity = %.3f, want 1", match.Score)
	}
	if len(match.Matches) == 0 || len(match.Mismatches) != 0 {
		t.Fatalf("exact similarity explanation = %+v", match)
	}

	mismatch := exact
	mismatch.GPUVendor = "amd"
	mismatch.GPUModel = "radeon rx 7800 xt"
	mismatch.DistroFamily = "fedora"
	result := SystemSimilarity(user, mismatch)
	if result.Score >= 0.75 {
		t.Fatalf("mismatch similarity = %.3f, want lower than exact", result.Score)
	}
	if len(result.Mismatches) == 0 || !strings.Contains(result.Summary, "differs") {
		t.Fatalf("mismatch explanation = %+v", result)
	}
}

func TestRankReportsRecencyBeatsVeryOldExactMatch(t *testing.T) {
	now := time.Date(2026, time.June, 1, 0, 0, 0, 0, time.UTC)
	profile := core.SystemProfile{Normalized: core.NormalizedSystemProfile{
		GPUVendor:    "nvidia",
		GPUModel:     "geforce rtx 4070",
		DistroFamily: "arch",
		Kernel:       "6.14",
		RAMBucket:    "32+",
	}}
	reports := []core.Report{
		reportWithSystem("old-exact", time.Date(2023, time.May, 1, 0, 0, 0, 0, time.UTC), "platinum", "PROTON_USE_WINED3D=1 %command%", map[string]string{
			"gpuVendor": "NVIDIA", "gpuModel": "GeForce RTX 4070", "distro": "Arch Linux", "kernel": "6.14", "ram": "32 GB",
		}),
		reportWithSystem("fresh-mismatch", time.Date(2026, time.May, 20, 0, 0, 0, 0, time.UTC), "gold", "RADV_PERFTEST=gpl %command%", map[string]string{
			"gpuVendor": "AMD", "gpuModel": "Radeon RX 7800 XT", "distro": "Fedora Linux", "kernel": "6.14", "ram": "32 GB",
		}),
	}

	ranked := RankReports(reports, profile, now)
	if len(ranked) != 2 {
		t.Fatalf("ranked len = %d", len(ranked))
	}
	if ranked[0].Report.SourceReportID != "fresh-mismatch" {
		t.Fatalf("top report = %s, want fresh mismatch over very old exact", ranked[0].Report.SourceReportID)
	}
	if ranked[1].SystemSimilarity <= ranked[0].SystemSimilarity {
		t.Fatalf("old exact similarity %.3f should explain more match than fresh %.3f", ranked[1].SystemSimilarity, ranked[0].SystemSimilarity)
	}
	if ranked[1].Freshness != core.FreshnessHistorical {
		t.Fatalf("old exact freshness = %q, want historical", ranked[1].Freshness)
	}
}

func TestExtractSuggestionsGroupsCitesAndFlagsConflicts(t *testing.T) {
	now := time.Date(2026, time.June, 1, 0, 0, 0, 0, time.UTC)
	profile := core.SystemProfile{Normalized: core.NormalizedSystemProfile{GPUVendor: "nvidia", DistroFamily: "arch"}}
	reports := []core.Report{
		reportWithSystem("r1", time.Date(2026, time.May, 20, 0, 0, 0, 0, time.UTC), "gold", "PROTON_ENABLE_NVAPI=1 %command%", map[string]string{"gpuVendor": "NVIDIA", "distro": "Arch Linux"}),
		reportWithNotes("r2", time.Date(2026, time.May, 21, 0, 0, 0, 0, time.UTC), "gold", "proton_enable_nvapi=1 %command%", "Use gamemoderun %command%. Try DXVK_ASYNC=1 if you see a black screen."),
		reportWithSystem("r3", time.Date(2026, time.May, 22, 0, 0, 0, 0, time.UTC), "silver", "PROTON_ENABLE_NVAPI=0 %command%", map[string]string{"gpuVendor": "NVIDIA", "distro": "Arch Linux"}),
	}
	suggestions := ExtractSuggestions(RankReports(reports, profile, now))

	one := findSuggestion(t, suggestions, core.SuggestionKindLaunchOption, "PROTON_ENABLE_NVAPI=1 %command%")
	if one.Occurrences != 2 {
		t.Fatalf("deduped occurrence count = %d, want 2", one.Occurrences)
	}
	if len(one.Sources) != 2 || one.Sources[0].ReportID == "" {
		t.Fatalf("sources not retained: %+v", one.Sources)
	}
	zero := findSuggestion(t, suggestions, core.SuggestionKindLaunchOption, "PROTON_ENABLE_NVAPI=0 %command%")
	if len(one.ConflictNotes) == 0 || len(zero.ConflictNotes) == 0 {
		t.Fatalf("conflict notes missing: one=%+v zero=%+v", one.ConflictNotes, zero.ConflictNotes)
	}
	_ = findSuggestion(t, suggestions, core.SuggestionKindEnvVar, "DXVK_ASYNC=1")
	_ = findSuggestion(t, suggestions, core.SuggestionKindWrapper, "gamemoderun")
	_ = findSuggestion(t, suggestions, core.SuggestionKindDiagnostic, "black screen")
}

func TestExtractSuggestionsPreservesQuotedEnvAssignments(t *testing.T) {
	now := time.Date(2026, time.June, 1, 0, 0, 0, 0, time.UTC)
	profile := core.SystemProfile{Normalized: core.NormalizedSystemProfile{GPUVendor: "nvidia", DistroFamily: "arch"}}
	reports := []core.Report{
		reportWithNotes("quoted-env", time.Date(2026, time.May, 20, 0, 0, 0, 0, time.UTC), "gold", "", `Use WINEDLLOVERRIDES="dxgi=n,b" %command% if DXGI fails.`),
	}
	suggestions := ExtractSuggestions(RankReports(reports, profile, now))

	launch := findSuggestion(t, suggestions, core.SuggestionKindLaunchOption, `WINEDLLOVERRIDES="dxgi=n,b" %command%`)
	if launch.Snippet != `WINEDLLOVERRIDES="dxgi=n,b" %command%` {
		t.Fatalf("launch snippet = %q", launch.Snippet)
	}
	env := findSuggestion(t, suggestions, core.SuggestionKindEnvVar, `WINEDLLOVERRIDES="dxgi=n,b"`)
	if env.Snippet != `WINEDLLOVERRIDES="dxgi=n,b"` {
		t.Fatalf("env snippet = %q", env.Snippet)
	}
	if len(env.Sources) != 1 || env.Sources[0].Snippet != `WINEDLLOVERRIDES="dxgi=n,b"` {
		t.Fatalf("env citations did not preserve exact snippet: %+v", env.Sources)
	}
}

func TestBuildLaunchPreviewPreservesQuotedEnvAssignments(t *testing.T) {
	preview := BuildLaunchPreview([]core.Suggestion{
		{ID: "env-quoted", Kind: core.SuggestionKindEnvVar, Snippet: `WINEDLLOVERRIDES="dxgi=n,b"`},
	}, "")
	if preview.Preview != `WINEDLLOVERRIDES="dxgi=n,b" %command%` {
		t.Fatalf("preview = %q", preview.Preview)
	}
}

func TestBuildLaunchPreviewComposesCommandOnceAndSkipsNotes(t *testing.T) {
	selected := []core.Suggestion{
		{ID: "env-1", Kind: core.SuggestionKindEnvVar, Snippet: "MANGOHUD=1"},
		{ID: "wrap-1", Kind: core.SuggestionKindWrapper, Snippet: "gamemoderun"},
		{ID: "note-1", Kind: core.SuggestionKindNote, Snippet: "No tweaks required"},
	}
	preview := BuildLaunchPreview(selected, "-novid")
	if strings.Count(preview.Preview, "%command%") != 1 {
		t.Fatalf("preview = %q, want one %%command%%", preview.Preview)
	}
	if preview.Preview != "MANGOHUD=1 gamemoderun %command% -novid" {
		t.Fatalf("preview = %q", preview.Preview)
	}
	if len(preview.Skipped) != 1 || preview.Skipped[0].ID != "note-1" {
		t.Fatalf("skipped = %+v, want note skipped", preview.Skipped)
	}
	if len(preview.Warnings) == 0 {
		t.Fatal("expected warning for existing options without command token and skipped note")
	}
}

func TestGenerateRecommendationKeepsSuggestionCitations(t *testing.T) {
	now := time.Date(2026, time.June, 1, 0, 0, 0, 0, time.UTC)
	profile := core.SystemProfile{Normalized: core.NormalizedSystemProfile{GPUVendor: "amd", DistroFamily: "fedora"}}
	game := core.Game{AppID: 123, Name: "Fixture Quest", Launcher: core.LauncherSteam}
	reports := []core.Report{
		reportWithSystem("fresh", time.Date(2026, time.May, 20, 0, 0, 0, 0, time.UTC), "gold", "RADV_PERFTEST=gpl %command%", map[string]string{"gpuVendor": "AMD", "distro": "Fedora Linux"}),
	}
	recommendation := GenerateRecommendation(game, reports, profile, now)
	if recommendation.GeneratedBy != core.RecommendationSourceRules {
		t.Fatalf("GeneratedBy = %q", recommendation.GeneratedBy)
	}
	if len(recommendation.Suggestions) == 0 {
		t.Fatal("expected suggestions")
	}
	if len(recommendation.Citations) == 0 {
		t.Fatal("expected top-level citations")
	}
	wantReportID := recommendation.Suggestions[0].Sources[0].ReportID
	found := false
	for _, citation := range recommendation.Citations {
		if citation.ReportID == wantReportID {
			found = true
			break
		}
	}
	if !found {
		t.Fatalf("top-level citations %+v do not include suggestion source %q", recommendation.Citations, wantReportID)
	}
}

func reportWithSystem(id string, timestamp time.Time, rating string, launchOptions string, systemInfo map[string]string) core.Report {
	return core.Report{
		SourceReportID: id,
		AppID:          123,
		Title:          "Fixture Quest",
		Timestamp:      timestamp,
		Rating:         rating,
		Notes:          "Launch option from report.",
		LaunchOptions:  launchOptions,
		SourceID:       "protondb-data:test",
		SystemInfo:     systemInfo,
	}
}

func reportWithNotes(id string, timestamp time.Time, rating string, launchOptions string, notes string) core.Report {
	report := reportWithSystem(id, timestamp, rating, launchOptions, map[string]string{"gpuVendor": "NVIDIA", "distro": "Arch Linux"})
	report.Notes = notes
	return report
}

func findSuggestion(t *testing.T, suggestions []core.Suggestion, kind string, snippet string) core.Suggestion {
	t.Helper()
	for _, suggestion := range suggestions {
		if suggestion.Kind == kind && canonicalSnippet(suggestion.Snippet) == canonicalSnippet(snippet) {
			return suggestion
		}
	}
	t.Fatalf("suggestion %s %q not found in %+v", kind, snippet, suggestions)
	return core.Suggestion{}
}
