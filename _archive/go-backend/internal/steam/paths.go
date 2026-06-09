package steam

import (
	"os"
	"path/filepath"
	"sort"
)

// CandidateRoots returns common Linux Steam roots for a home directory.
func CandidateRoots(home string) []string {
	if home == "" {
		return nil
	}
	return dedupePaths([]string{
		filepath.Join(home, ".steam", "steam"),
		filepath.Join(home, ".steam", "root"),
		filepath.Join(home, ".local", "share", "Steam"),
		filepath.Join(home, ".var", "app", "com.valvesoftware.Steam", ".local", "share", "Steam"),
	})
}

// ExistingRoots returns existing common Linux Steam roots for the current user.
func ExistingRoots() []string {
	home, err := os.UserHomeDir()
	if err != nil {
		return nil
	}
	seenReal := map[string]bool{}
	var roots []string
	for _, candidate := range CandidateRoots(home) {
		if info, err := os.Stat(candidate); err == nil && info.IsDir() {
			realPath, err := filepath.EvalSymlinks(candidate)
			if err != nil {
				realPath = candidate
			}
			key := filepath.Clean(realPath)
			if seenReal[key] {
				continue
			}
			seenReal[key] = true
			roots = append(roots, key)
		}
	}
	roots = dedupePaths(roots)
	sort.Strings(roots)
	return roots
}

func dedupePaths(paths []string) []string {
	seen := map[string]bool{}
	var out []string
	for _, path := range paths {
		if path == "" {
			continue
		}
		cleaned := filepath.Clean(path)
		key := cleaned
		if realPath, err := filepath.EvalSymlinks(cleaned); err == nil {
			key = filepath.Clean(realPath)
		}
		if seen[key] {
			continue
		}
		seen[key] = true
		out = append(out, key)
	}
	return out
}
