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
	steamapps := filepath.Join(root, "steamapps")
	if err := os.MkdirAll(steamapps, 0o755); err != nil {
		t.Fatal(err)
	}

	libraryFolders := "\"libraryfolders\" { \"0\" { \"path\" \"" + filepath.ToSlash(root) + "\" } }"
	if err := os.WriteFile(filepath.Join(steamapps, "libraryfolders.vdf"), []byte(libraryFolders), 0o644); err != nil {
		t.Fatal(err)
	}
	manifest, err := os.ReadFile("../../testdata/steam/appmanifest_123.acf")
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(steamapps, "appmanifest_123.acf"), manifest, 0o644); err != nil {
		t.Fatal(err)
	}

	games, err := ScanRoot(root)
	if err != nil {
		t.Fatal(err)
	}
	if len(games) != 1 {
		t.Fatalf("len(games) = %d", len(games))
	}
	if games[0].AppID != 123 || games[0].Name != "Puzzle Proton" {
		t.Fatalf("unexpected game: %+v", games[0])
	}
}
