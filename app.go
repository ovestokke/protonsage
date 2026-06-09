//go:build wails

package main

import (
	"context"

	appsvc "protonsage/internal/app"
	"protonsage/internal/core"
)

// App is the thin Wails binding layer. Keep behavior in internal packages.
type App struct {
	ctx context.Context
	svc *appsvc.Service
}

func NewApp() *App {
	return &App{svc: appsvc.NewService()}
}

func (a *App) startup(ctx context.Context) {
	a.ctx = ctx
}

func (a *App) GetAppInfo() appsvc.AppInfo {
	return a.svc.GetAppInfo()
}

func (a *App) GetSystemProfile() core.SystemProfile {
	return a.svc.GetSystemProfile(a.ctx)
}

func (a *App) ScanSteam(root string) ([]core.Game, error) {
	return a.svc.ScanSteam(a.ctx, root)
}

func (a *App) GetInstalledGames(dbPath string, root string) ([]appsvc.InstalledGameStatus, error) {
	return a.svc.GetInstalledGames(a.ctx, dbPath, root)
}

func (a *App) GetRecommendation(dbPath string, appid int) (core.Recommendation, error) {
	return a.svc.GetRecommendation(a.ctx, dbPath, appid)
}

func (a *App) BuildLaunchPreview(selected []core.Suggestion, existing string) core.PreviewResult {
	return a.svc.BuildLaunchPreview(a.ctx, selected, existing)
}
