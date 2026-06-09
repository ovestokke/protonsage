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

// Game is the normalized local game model used by CLI, storage, advisor and UI.
type Game struct {
	AppID       int      `json:"appId"`
	Name        string   `json:"name"`
	InstallPath string   `json:"installPath,omitempty"`
	LibraryPath string   `json:"libraryPath,omitempty"`
	Launcher    Launcher `json:"launcher"`
	SizeOnDisk  int64    `json:"sizeOnDisk,omitempty"`
	StateFlags  int64    `json:"stateFlags,omitempty"`
	BuildID     string   `json:"buildId,omitempty"`
}

// Report represents a ProtonDB report after import/normalization.
type Report struct {
	AppID         int               `json:"appId"`
	Title         string            `json:"title,omitempty"`
	Timestamp     time.Time         `json:"timestamp"`
	Verdict       string            `json:"verdict,omitempty"`
	Rating        string            `json:"rating,omitempty"`
	Notes         string            `json:"notes,omitempty"`
	LaunchOptions string            `json:"launchOptions,omitempty"`
	ProtonVersion string            `json:"protonVersion,omitempty"`
	SystemInfo    map[string]string `json:"systemInfo,omitempty"`
	SourceID      string            `json:"sourceId"`
}

// SystemProfile is a read-only normalized view of the user's local system.
type SystemProfile struct {
	GPUVendor   string            `json:"gpuVendor,omitempty"`
	GPUModel    string            `json:"gpuModel,omitempty"`
	GPUDriver   string            `json:"gpuDriver,omitempty"`
	CPU         string            `json:"cpu,omitempty"`
	RAMGB       float64           `json:"ramGb,omitempty"`
	Distro      string            `json:"distro,omitempty"`
	Kernel      string            `json:"kernel,omitempty"`
	SessionType string            `json:"sessionType,omitempty"`
	Desktop     string            `json:"desktop,omitempty"`
	Raw         map[string]string `json:"raw,omitempty"`
}

// Citation ties user-visible recommendations back to source reports/snippets.
type Citation struct {
	SourceID  string `json:"sourceId"`
	ReportID  string `json:"reportId,omitempty"`
	AppID     int    `json:"appId,omitempty"`
	Timestamp string `json:"timestamp,omitempty"`
	Snippet   string `json:"snippet,omitempty"`
}

// Suggestion is a selectable recommendation/workaround candidate.
type Suggestion struct {
	Snippet          string     `json:"snippet"`
	Kind             string     `json:"kind"`
	Sources          []Citation `json:"sources,omitempty"`
	Occurrences      int        `json:"occurrences"`
	RecencyScore     float64    `json:"recencyScore"`
	SystemSimilarity float64    `json:"systemSimilarity"`
	Confidence       string     `json:"confidence"`
	ConflictNotes    []string   `json:"conflictNotes,omitempty"`
}

// Recommendation is the structured output consumed by CLI/UI.
type Recommendation struct {
	Game        Game                 `json:"game"`
	Summary     string               `json:"summary"`
	Suggestions []Suggestion         `json:"suggestions"`
	Citations   []Citation           `json:"citations,omitempty"`
	GeneratedBy RecommendationSource `json:"generatedBy"`
}
