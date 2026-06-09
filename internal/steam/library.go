package steam

import (
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"

	"protonsage/internal/core"
)

// LibraryFoldersFromRoot reads steamapps/libraryfolders.vdf and returns library roots.
func LibraryFoldersFromRoot(root string) ([]string, error) {
	root = filepath.Clean(root)
	steamapps := filepath.Join(root, "steamapps")
	vdfPath := filepath.Join(steamapps, "libraryfolders.vdf")

	obj, err := ParseVDFFile(vdfPath)
	if err != nil {
		if os.IsNotExist(err) {
			if info, statErr := os.Stat(steamapps); statErr == nil && info.IsDir() {
				return []string{root}, nil
			}
		}
		return nil, err
	}

	foldersObj, ok := AsObject(obj["libraryfolders"])
	if !ok {
		return nil, fmt.Errorf("%s: missing libraryfolders object", vdfPath)
	}

	folders := []string{root}
	for _, value := range foldersObj {
		entry, ok := AsObject(value)
		if !ok {
			continue
		}
		path := StringValue(entry, "path")
		if path != "" {
			folders = append(folders, path)
		}
	}
	folders = dedupePaths(folders)
	sort.Strings(folders)
	return folders, nil
}

// ScanRoot scans a Steam root read-only and returns installed games from app manifests.
func ScanRoot(root string) ([]core.Game, error) {
	libraries, err := LibraryFoldersFromRoot(root)
	if err != nil {
		return nil, err
	}
	launchOptions, err := LaunchOptionsFromRoot(root)
	if err != nil {
		// Existing launch options are optional display context; a malformed localconfig
		// should not prevent the read-only installed-game scan from succeeding.
		launchOptions = map[int]string{}
	}

	var games []core.Game
	for _, library := range libraries {
		libraryGames, err := ScanLibrary(library)
		if err != nil {
			// Missing or inaccessible secondary libraries should not hide valid games from other libraries.
			if os.IsNotExist(err) {
				continue
			}
			return nil, err
		}
		for i := range libraryGames {
			if value := launchOptions[libraryGames[i].AppID]; value != "" {
				libraryGames[i].ExistingLaunchOptions = value
			}
		}
		games = append(games, libraryGames...)
	}
	sort.SliceStable(games, func(i, j int) bool {
		if games[i].Name == games[j].Name {
			return games[i].AppID < games[j].AppID
		}
		return strings.ToLower(games[i].Name) < strings.ToLower(games[j].Name)
	})
	return games, nil
}

// ScanLibrary scans one Steam library root read-only.
func ScanLibrary(libraryPath string) ([]core.Game, error) {
	pattern := filepath.Join(libraryPath, "steamapps", "appmanifest_*.acf")
	matches, err := filepath.Glob(pattern)
	if err != nil {
		return nil, err
	}
	if len(matches) == 0 {
		if _, statErr := os.Stat(filepath.Join(libraryPath, "steamapps")); statErr != nil {
			return nil, statErr
		}
		return nil, nil
	}
	sort.Strings(matches)

	games := make([]core.Game, 0, len(matches))
	for _, manifest := range matches {
		game, err := ParseAppManifestFile(manifest, libraryPath)
		if err != nil {
			return nil, err
		}
		games = append(games, game)
	}
	return games, nil
}

// ParseAppManifestFile parses one appmanifest_*.acf file read-only.
func ParseAppManifestFile(path string, libraryPath string) (core.Game, error) {
	obj, err := ParseVDFFile(path)
	if err != nil {
		return core.Game{}, err
	}
	return ParseAppManifest(obj, libraryPath)
}

// ParseAppManifest converts an AppState VDF object to a Game.
func ParseAppManifest(obj VDFObject, libraryPath string) (core.Game, error) {
	state, ok := AsObject(obj["AppState"])
	if !ok {
		return core.Game{}, fmt.Errorf("missing AppState object")
	}

	appid, err := parseInt(StringValue(state, "appid"))
	if err != nil {
		return core.Game{}, fmt.Errorf("invalid appid: %w", err)
	}
	sizeOnDisk, _ := parseInt64(StringValue(state, "SizeOnDisk"))
	stateFlags, _ := parseInt64(StringValue(state, "StateFlags"))
	installDir := StringValue(state, "installdir")

	game := core.Game{
		AppID:       appid,
		Name:        StringValue(state, "name"),
		LibraryPath: filepath.Clean(libraryPath),
		Launcher:    core.LauncherSteam,
		SizeOnDisk:  sizeOnDisk,
		StateFlags:  stateFlags,
		BuildID:     StringValue(state, "buildid"),
	}
	if installDir != "" {
		game.InstallPath = filepath.Join(game.LibraryPath, "steamapps", "common", installDir)
	}
	return game, nil
}

func parseInt(value string) (int, error) {
	parsed, err := strconv.Atoi(strings.TrimSpace(value))
	if err != nil {
		return 0, err
	}
	return parsed, nil
}

func parseInt64(value string) (int64, error) {
	if strings.TrimSpace(value) == "" {
		return 0, nil
	}
	return strconv.ParseInt(strings.TrimSpace(value), 10, 64)
}
