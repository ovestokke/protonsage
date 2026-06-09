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
