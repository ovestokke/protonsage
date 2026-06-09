package steam

import (
	"os"
	"path/filepath"
	"testing"
)

func TestParseAppManifest(t *testing.T) {
	game, err := ParseAppManifestFile("../../testdata/steam/appmanifest_123.acf", "/tmp/SteamLibrary")
	if err != nil {
		t.Fatal(err)
	}
	if game.AppID != 123 {
		t.Fatalf("AppID = %d", game.AppID)
	}
	if game.Name != "Puzzle Proton" {
		t.Fatalf("Name = %q", game.Name)
	}
	if game.InstallPath != filepath.Join("/tmp/SteamLibrary", "steamapps", "common", "Puzzle Proton") {
		t.Fatalf("InstallPath = %q", game.InstallPath)
	}
	if game.SizeOnDisk != 424242 || game.StateFlags != 4 || game.BuildID != "999" {
		t.Fatalf("unexpected metadata: %+v", game)
	}
}

func TestScanRootReadOnlyWithFixture(t *testing.T) {
	root := t.TempDir()
	secondary := t.TempDir()
	steamapps := filepath.Join(root, "steamapps")
	secondarySteamapps := filepath.Join(secondary, "steamapps")
	if err := os.MkdirAll(steamapps, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.MkdirAll(secondarySteamapps, 0o755); err != nil {
		t.Fatal(err)
	}

	libraryFolders := "\"libraryfolders\" { \"0\" { \"path\" \"" + filepath.ToSlash(root) + "\" } \"1\" { \"path\" \"" + filepath.ToSlash(secondary) + "\" } }"
	if err := os.WriteFile(filepath.Join(steamapps, "libraryfolders.vdf"), []byte(libraryFolders), 0o644); err != nil {
		t.Fatal(err)
	}
	manifest123, err := os.ReadFile("../../testdata/steam/appmanifest_123.acf")
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(steamapps, "appmanifest_123.acf"), manifest123, 0o644); err != nil {
		t.Fatal(err)
	}
	manifest456, err := os.ReadFile("../../testdata/steam/appmanifest_456.acf")
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(secondarySteamapps, "appmanifest_456.acf"), manifest456, 0o644); err != nil {
		t.Fatal(err)
	}
	localConfig, err := os.ReadFile("../../testdata/steam/localconfig.vdf")
	if err != nil {
		t.Fatal(err)
	}
	localConfigDir := filepath.Join(root, "userdata", "76561198000000000", "config")
	if err := os.MkdirAll(localConfigDir, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(localConfigDir, "localconfig.vdf"), localConfig, 0o644); err != nil {
		t.Fatal(err)
	}

	libraries, err := LibraryFoldersFromRoot(root)
	if err != nil {
		t.Fatal(err)
	}
	if len(libraries) != 2 {
		t.Fatalf("len(libraries) = %d, want root + secondary: %v", len(libraries), libraries)
	}

	games, err := ScanRoot(root)
	if err != nil {
		t.Fatal(err)
	}
	if len(games) != 2 {
		t.Fatalf("len(games) = %d", len(games))
	}
	byAppID := map[int]anyGame{}
	for _, game := range games {
		byAppID[game.AppID] = anyGame{Name: game.Name, LaunchOptions: game.ExistingLaunchOptions, LibraryPath: game.LibraryPath}
	}
	if byAppID[123].Name != "Puzzle Proton" || byAppID[123].LaunchOptions != "PROTON_USE_WINED3D=1 %command%" {
		t.Fatalf("unexpected app 123: %+v", byAppID[123])
	}
	if byAppID[456].Name != "Sample Tactics" || byAppID[456].LaunchOptions != "gamemoderun %command%" {
		t.Fatalf("unexpected app 456: %+v", byAppID[456])
	}
}

type anyGame struct {
	Name          string
	LaunchOptions string
	LibraryPath   string
}
