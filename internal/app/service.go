package app

import (
	"context"

	"protonsage/internal/core"
	"protonsage/internal/steam"
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
			"read-only local system profile",
			"ProtonDB latest snapshot metadata lookup",
			"copy/export first; no Steam config writes",
		},
	}
}

func (s *Service) GetSystemProfile(_ context.Context) core.SystemProfile {
	return system.DetectProfile()
}

func (s *Service) ScanSteamRoot(_ context.Context, root string) ([]core.Game, error) {
	return steam.ScanRoot(root)
}
