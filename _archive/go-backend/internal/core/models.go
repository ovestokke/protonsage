package core

import "time"

// Launcher identifies where a game entry came from.
type Launcher string

const (
	LauncherSteam Launcher = "steam"
)

// RecommendationSource identifies how a recommendation was generated.
type RecommendationSource string

const (
	RecommendationSourceRules RecommendationSource = "rules"
	RecommendationSourceAI    RecommendationSource = "ai"
)

const (
	SuggestionKindLaunchOption = "launch_option"
	SuggestionKindEnvVar       = "env_var"
	SuggestionKindWrapper      = "wrapper"
	SuggestionKindWorkaround   = "workaround"
	SuggestionKindDiagnostic   = "diagnostic"
	SuggestionKindNote         = "note"
)

const (
	SuggestionConfidenceHigh   = "high"
	SuggestionConfidenceMedium = "medium"
	SuggestionConfidenceLow    = "low"
)

const (
	FreshnessFresh      = "fresh"
	FreshnessRecent     = "recent"
	FreshnessStale      = "stale"
	FreshnessHistorical = "historical"
)

// Game is the normalized local game model used by CLI, storage, advisor and UI.
type Game struct {
	AppID                 int      `json:"appId"`
	Name                  string   `json:"name"`
	InstallPath           string   `json:"installPath,omitempty"`
	LibraryPath           string   `json:"libraryPath,omitempty"`
	Launcher              Launcher `json:"launcher"`
	SizeOnDisk            int64    `json:"sizeOnDisk,omitempty"`
	StateFlags            int64    `json:"stateFlags,omitempty"`
	BuildID               string   `json:"buildId,omitempty"`
	ExistingLaunchOptions string   `json:"existingLaunchOptions,omitempty"`
}

// Report represents a ProtonDB report after import/normalization.
type Report struct {
	ID             int64             `json:"id,omitempty"`
	SourceReportID string            `json:"sourceReportId,omitempty"`
	AppID          int               `json:"appId"`
	Title          string            `json:"title,omitempty"`
	Timestamp      time.Time         `json:"timestamp"`
	Verdict        string            `json:"verdict,omitempty"`
	Rating         string            `json:"rating,omitempty"`
	Notes          string            `json:"notes,omitempty"`
	LaunchOptions  string            `json:"launchOptions,omitempty"`
	ProtonVersion  string            `json:"protonVersion,omitempty"`
	SystemInfo     map[string]string `json:"systemInfo,omitempty"`
	SourceID       string            `json:"sourceId"`
}

// SystemProfile is a read-only view of the user's local system plus comparable normalized categories.
type SystemProfile struct {
	GPUVendor   string                  `json:"gpuVendor,omitempty"`
	GPUModel    string                  `json:"gpuModel,omitempty"`
	GPUDriver   string                  `json:"gpuDriver,omitempty"`
	CPU         string                  `json:"cpu,omitempty"`
	RAMGB       float64                 `json:"ramGb,omitempty"`
	Distro      string                  `json:"distro,omitempty"`
	Kernel      string                  `json:"kernel,omitempty"`
	SessionType string                  `json:"sessionType,omitempty"`
	Desktop     string                  `json:"desktop,omitempty"`
	Normalized  NormalizedSystemProfile `json:"normalized"`
	Raw         map[string]string       `json:"raw,omitempty"`
}

// NormalizedSystemProfile contains coarse categories for local/report system comparison.
type NormalizedSystemProfile struct {
	GPUVendor    string `json:"gpuVendor,omitempty"`
	GPUModel     string `json:"gpuModel,omitempty"`
	GPUDriver    string `json:"gpuDriver,omitempty"`
	CPUVendor    string `json:"cpuVendor,omitempty"`
	CPUClass     string `json:"cpuClass,omitempty"`
	RAMBucket    string `json:"ramBucket,omitempty"`
	DistroFamily string `json:"distroFamily,omitempty"`
	Kernel       string `json:"kernel,omitempty"`
	SessionType  string `json:"sessionType,omitempty"`
}

// Citation ties user-visible recommendations back to source reports/snippets.
type Citation struct {
	SourceID  string `json:"sourceId"`
	ReportID  string `json:"reportId,omitempty"`
	AppID     int    `json:"appId,omitempty"`
	Timestamp string `json:"timestamp,omitempty"`
	Snippet   string `json:"snippet,omitempty"`
}

// SimilarityResult explains how a report system profile compares with the local profile.
type SimilarityResult struct {
	Score      float64  `json:"score"`
	Matches    []string `json:"matches,omitempty"`
	Mismatches []string `json:"mismatches,omitempty"`
	Unknowns   []string `json:"unknowns,omitempty"`
	Summary    string   `json:"summary"`
}

// RankedReport is a ProtonDB report with deterministic advisor scoring metadata.
type RankedReport struct {
	Report           Report           `json:"report"`
	Score            float64          `json:"score"`
	RecencyScore     float64          `json:"recencyScore"`
	SystemSimilarity float64          `json:"systemSimilarity"`
	QualityScore     float64          `json:"qualityScore"`
	Freshness        string           `json:"freshness"`
	AgeDays          int              `json:"ageDays"`
	Similarity       SimilarityResult `json:"similarity"`
	Reasons          []string         `json:"reasons,omitempty"`
}

// Suggestion is a selectable recommendation/workaround candidate.
type Suggestion struct {
	ID               string     `json:"id"`
	Snippet          string     `json:"snippet"`
	Kind             string     `json:"kind"`
	Sources          []Citation `json:"sources,omitempty"`
	Occurrences      int        `json:"occurrences"`
	RecencyScore     float64    `json:"recencyScore"`
	SystemSimilarity float64    `json:"systemSimilarity"`
	Confidence       string     `json:"confidence"`
	ConflictNotes    []string   `json:"conflictNotes,omitempty"`
}

// PreviewResult is the copy/export-only launch-option composition output.
type PreviewResult struct {
	Existing  string       `json:"existing,omitempty"`
	Preview   string       `json:"preview"`
	Applied   []Suggestion `json:"applied,omitempty"`
	Skipped   []Suggestion `json:"skipped,omitempty"`
	Warnings  []string     `json:"warnings,omitempty"`
	Conflicts []string     `json:"conflicts,omitempty"`
}

// Recommendation is the structured output consumed by CLI/UI.
type Recommendation struct {
	Game          Game                 `json:"game"`
	Summary       string               `json:"summary"`
	Suggestions   []Suggestion         `json:"suggestions"`
	Citations     []Citation           `json:"citations,omitempty"`
	RankedReports []RankedReport       `json:"rankedReports,omitempty"`
	Warnings      []string             `json:"warnings,omitempty"`
	GeneratedBy   RecommendationSource `json:"generatedBy"`
}
