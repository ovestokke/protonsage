//go:build wails

package main

import (
	"context"
	"os"
	"path/filepath"

	appsvc "protonsage/internal/app"
	"protonsage/internal/core"
)

// App is the thin Wails binding layer. Keep behavior in internal packages.
type App struct {
	ctx context.Context
	svc *appsvc.Service
	dbPath string
}

func NewApp() *App {
	return &App{svc: appsvc.NewService(), dbPath: defaultDBPath()}
}

func (a *App) startup(ctx context.Context) {
	a.ctx = ctx
}

// DbPath returns the configured database path so the frontend can pass it to backend methods.
func (a *App) DbPath() string { return a.dbPath }

func (a *App) GetAppInfo() appsvc.AppInfo {
	return a.svc.GetAppInfo()
}

func (a *App) GetSystemProfile() core.SystemProfile {
	return a.svc.GetSystemProfile(a.ctx)
}

func (a *App) GetDataStatus() (appsvc.DataStatus, error) {
	return a.svc.GetDataStatus(a.ctx, a.dbPath)
}

func (a *App) ScanSteam(root string) ([]core.Game, error) {
	return a.svc.ScanSteam(a.ctx, root)
}

func (a *App) GetInstalledGames() ([]appsvc.InstalledGameStatus, error) {
	return a.svc.GetInstalledGames(a.ctx, a.dbPath, "")
}

func (a *App) SearchGames(query string, limit int) ([]core.Game, error) {
	return a.svc.SearchGames(a.ctx, a.dbPath, query, limit)
}

func (a *App) GetRecommendation(appid int) (core.Recommendation, error) {
	return a.svc.GetRecommendation(a.ctx, a.dbPath, appid)
}

func (a *App) BuildLaunchPreview(selected []core.Suggestion, existing string) core.PreviewResult {
	return a.svc.BuildLaunchPreview(a.ctx, selected, existing)
}

// defaultDBPath returns the XDG data directory path for ProtonSage.
func defaultDBPath() string {
	dataDir := os.Getenv("XDG_DATA_HOME")
	if dataDir == "" {
		home, err := os.UserHomeDir()
		if err != nil || home == "" {
			dataDir = "/tmp"
		} else {
			dataDir = filepath.Join(home, ".local", "share")
		}
	}
	dir := filepath.Join(dataDir, "protonsage")
	_ = os.MkdirAll(dir, 0755)
	return filepath.Join(dir, "protonsage.db")
}
