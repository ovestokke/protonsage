package app

import (
	"context"
	"fmt"
	"sort"
	"strings"
	"time"

	"protonsage/internal/advisor"
	"protonsage/internal/core"
	"protonsage/internal/steam"
	"protonsage/internal/storage"
	"protonsage/internal/system"
)

const Version = "0.1.0-dev"

// AppInfo is safe to expose to CLI and Wails frontend.
type AppInfo struct {
	Name         string   `json:"name"`
	Version      string   `json:"version"`
	Stack        string   `json:"stack"`
	Capabilities []string `json:"capabilities"`
}

// DataStatus is safe to expose through CLI/Wails without leaking storage internals.
type DataStatus struct {
	DBPath         string             `json:"dbPath"`
	SourceCount    int                `json:"sourceCount"`
	ImportRunCount int                `json:"importRunCount"`
	GameCount      int                `json:"gameCount"`
	ReportCount    int                `json:"reportCount"`
	LatestImport   *storage.ImportRun `json:"latestImport,omitempty"`
}

// InstalledGameStatus combines read-only Steam scan data with local ProtonDB import availability.
type InstalledGameStatus struct {
	Game               core.Game  `json:"game"`
	Installed          bool       `json:"installed"`
	MatchKind          string     `json:"matchKind"`
	DataGame           *core.Game `json:"dataGame,omitempty"`
	ProtonDBAppID      int        `json:"protonDbAppId,omitempty"`
	HasProtonDBReports bool       `json:"hasProtonDbReports"`
	ReportCount        int        `json:"reportCount"`
}

// Service is the application facade shared by CLI and Wails bindings.
type Service struct{}

func NewService() *Service {
	return &Service{}
}

func (s *Service) GetAppInfo() AppInfo {
	return AppInfo{
		Name:    "ProtonSage",
		Version: Version,
		Stack:   "Go core + Wails desktop shell + modern React frontend",
		Capabilities: []string{
			"read-only Steam library scan",
			"read-only existing Steam launch-options context",
			"installed-game matching against imported ProtonDB data",
			"read-only local system profile with normalized comparison categories",
			"deterministic no-AI ProtonDB recommendation JSON with citations",
			"copy/export launch-option preview composition",
			"ProtonDB latest snapshot metadata lookup",
			"local ProtonDB fixture import into SQLite",
			"copy/export first; no Steam config writes",
		},
	}
}

func (s *Service) GetSystemProfile(_ context.Context) core.SystemProfile {
	return system.DetectProfile()
}

func (s *Service) ScanSteam(ctx context.Context, root string) ([]core.Game, error) {
	ctx = ensureContext(ctx)
	roots := []string{}
	if strings.TrimSpace(root) != "" {
		roots = append(roots, root)
	} else {
		roots = steam.ExistingRoots()
	}
	if len(roots) == 0 {
		return []core.Game{}, nil
	}

	var games []core.Game
	for _, scanRoot := range roots {
		select {
		case <-ctx.Done():
			return nil, ctx.Err()
		default:
		}
		rootGames, err := steam.ScanRoot(scanRoot)
		if err != nil {
			return nil, err
		}
		games = append(games, rootGames...)
	}
	sort.SliceStable(games, func(i, j int) bool {
		left := strings.ToLower(games[i].Name)
		right := strings.ToLower(games[j].Name)
		if left == right {
			return games[i].AppID < games[j].AppID
		}
		return left < right
	})
	return games, nil
}

func (s *Service) ScanSteamRoot(ctx context.Context, root string) ([]core.Game, error) {
	return s.ScanSteam(ctx, root)
}

func (s *Service) GetInstalledGames(ctx context.Context, dbPath string, root string) ([]InstalledGameStatus, error) {
	ctx = ensureContext(ctx)
	games, err := s.ScanSteam(ctx, root)
	if err != nil {
		return nil, err
	}
	if len(games) == 0 {
		return []InstalledGameStatus{}, nil
	}

	db, err := storage.Open(dbPath)
	if err != nil {
		return nil, err
	}
	defer db.Close()

	statuses := make([]InstalledGameStatus, 0, len(games))
	for _, game := range games {
		status, err := matchInstalledGame(ctx, db, game)
		if err != nil {
			return nil, err
		}
		statuses = append(statuses, status)
	}
	return statuses, nil
}

func (s *Service) GetDataStatus(ctx context.Context, dbPath string) (DataStatus, error) {
	ctx = ensureContext(ctx)
	db, err := storage.Open(dbPath)
	if err != nil {
		return DataStatus{}, err
	}
	defer db.Close()

	status, err := db.Status(ctx)
	if err != nil {
		return DataStatus{}, err
	}
	return DataStatus{
		DBPath:         dbPath,
		SourceCount:    status.SourceCount,
		ImportRunCount: status.ImportRunCount,
		GameCount:      status.GameCount,
		ReportCount:    status.ReportCount,
		LatestImport:   status.LatestImport,
	}, nil
}

func (s *Service) GetRecommendation(ctx context.Context, dbPath string, appid int) (core.Recommendation, error) {
	profile := s.GetSystemProfile(ctx)
	return s.getRecommendationForProfile(ctx, dbPath, appid, profile, time.Now().UTC())
}

func (s *Service) BuildLaunchPreview(_ context.Context, selected []core.Suggestion, existing string) core.PreviewResult {
	return advisor.BuildLaunchPreview(selected, existing)
}

func (s *Service) SearchGames(ctx context.Context, dbPath string, query string, limit int) ([]core.Game, error) {
	ctx = ensureContext(ctx)
	if strings.TrimSpace(query) == "" {
		return nil, nil
	}
	if limit <= 0 {
		limit = 20
	}
	db, err := storage.Open(dbPath)
	if err != nil {
		return nil, err
	}
	defer db.Close()
	return db.SearchGames(ctx, query, limit)
}

func (s *Service) getRecommendationForProfile(ctx context.Context, dbPath string, appid int, profile core.SystemProfile, now time.Time) (core.Recommendation, error) {
	ctx = ensureContext(ctx)
	if appid <= 0 {
		return core.Recommendation{}, fmt.Errorf("recommendation requires appid")
	}
	db, err := storage.Open(dbPath)
	if err != nil {
		return core.Recommendation{}, err
	}
	defer db.Close()

	game, ok, err := db.LookupGame(ctx, appid)
	if err != nil {
		return core.Recommendation{}, err
	}
	if !ok {
		return core.Recommendation{}, fmt.Errorf("appid %d was not found in imported data", appid)
	}

	records, err := db.ReportsByAppID(ctx, appid)
	if err != nil {
		return core.Recommendation{}, err
	}
	reports := make([]core.Report, 0, len(records))
	for _, record := range records {
		reports = append(reports, record.Report)
	}
	return advisor.GenerateRecommendation(game, reports, profile, now), nil
}

func matchInstalledGame(ctx context.Context, db *storage.DB, game core.Game) (InstalledGameStatus, error) {
	status := InstalledGameStatus{
		Game:      game,
		Installed: true,
		MatchKind: "none",
	}
	var matched core.Game
	if game.AppID > 0 {
		dataGame, ok, err := db.LookupGame(ctx, game.AppID)
		if err != nil {
			return InstalledGameStatus{}, err
		}
		if ok {
			matched = dataGame
			status.MatchKind = "appid"
		}
	}
	if status.MatchKind == "none" && strings.TrimSpace(game.Name) != "" {
		candidates, err := db.SearchGames(ctx, game.Name, 5)
		if err != nil {
			return InstalledGameStatus{}, err
		}
		for _, candidate := range candidates {
			if normalizedName(candidate.Name) == normalizedName(game.Name) {
				matched = candidate
				status.MatchKind = "name"
				break
			}
		}
	}
	if status.MatchKind == "none" {
		return status, nil
	}

	status.ProtonDBAppID = matched.AppID
	matchedCopy := matched
	status.DataGame = &matchedCopy
	count, err := db.ReportCountByAppID(ctx, matched.AppID)
	if err != nil {
		return InstalledGameStatus{}, err
	}
	status.ReportCount = count
	status.HasProtonDBReports = count > 0
	return status, nil
}

func normalizedName(value string) string {
	return strings.Join(strings.Fields(strings.ToLower(strings.TrimSpace(value))), " ")
}

func ensureContext(ctx context.Context) context.Context {
	if ctx == nil {
		return context.Background()
	}
	return ctx
}
