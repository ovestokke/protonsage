package protondb

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"regexp"
	"sort"
	"strings"
	"time"
)

const (
	ReportsContentsAPI = "https://api.github.com/repos/bdefore/protondb-data/contents/reports"
	ReportsRawBaseURL  = "https://raw.githubusercontent.com/bdefore/protondb-data/master/reports"
)

var snapshotNamePattern = regexp.MustCompile(`(?i)^reports_([a-z]{3})(\d{1,2})_(\d{4})\.tar\.gz$`)

var monthByAbbrev = map[string]time.Month{
	"jan": time.January,
	"feb": time.February,
	"mar": time.March,
	"apr": time.April,
	"may": time.May,
	"jun": time.June,
	"jul": time.July,
	"aug": time.August,
	"sep": time.September,
	"oct": time.October,
	"nov": time.November,
	"dec": time.December,
}

// Snapshot describes one reports_*.tar.gz archive in protondb-data.
type Snapshot struct {
	Filename string    `json:"filename"`
	Date     time.Time `json:"date"`
	URL      string    `json:"url,omitempty"`
	Size     int64     `json:"size,omitempty"`
}

// ParseSnapshotFilename extracts the date from names like reports_jun1_2026.tar.gz.
func ParseSnapshotFilename(filename string) (time.Time, bool) {
	matches := snapshotNamePattern.FindStringSubmatch(strings.TrimSpace(filename))
	if matches == nil {
		return time.Time{}, false
	}

	month, ok := monthByAbbrev[strings.ToLower(matches[1])]
	if !ok {
		return time.Time{}, false
	}

	var day, year int
	if _, err := fmt.Sscanf(matches[2], "%d", &day); err != nil {
		return time.Time{}, false
	}
	if _, err := fmt.Sscanf(matches[3], "%d", &year); err != nil {
		return time.Time{}, false
	}

	date := time.Date(year, month, day, 0, 0, 0, 0, time.UTC)
	if date.Day() != day || date.Month() != month || date.Year() != year {
		return time.Time{}, false
	}
	return date, true
}

// SelectLatestSnapshot selects the newest valid reports_*.tar.gz archive name.
func SelectLatestSnapshot(filenames []string) (Snapshot, bool) {
	var latest Snapshot
	found := false
	for _, filename := range filenames {
		date, ok := ParseSnapshotFilename(filename)
		if !ok {
			continue
		}
		if !found || date.After(latest.Date) || (date.Equal(latest.Date) && filename > latest.Filename) {
			latest = Snapshot{
				Filename: filename,
				Date:     date,
				URL:      ReportsRawBaseURL + "/" + filename,
			}
			found = true
		}
	}
	return latest, found
}

// SortSnapshotsNewestFirst sorts snapshots in-place by date descending.
func SortSnapshotsNewestFirst(snapshots []Snapshot) {
	sort.SliceStable(snapshots, func(i, j int) bool {
		if snapshots[i].Date.Equal(snapshots[j].Date) {
			return snapshots[i].Filename > snapshots[j].Filename
		}
		return snapshots[i].Date.After(snapshots[j].Date)
	})
}

type githubContentItem struct {
	Name        string `json:"name"`
	Size        int64  `json:"size"`
	DownloadURL string `json:"download_url"`
	Type        string `json:"type"`
}

// ListSnapshotsFromGitHub lists ProtonDB report archives through the GitHub contents API.
// It only reads directory metadata; it does not download report archives.
func ListSnapshotsFromGitHub(ctx context.Context, client *http.Client) ([]Snapshot, error) {
	if client == nil {
		client = http.DefaultClient
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, ReportsContentsAPI, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Accept", "application/vnd.github+json")
	req.Header.Set("User-Agent", "ProtonSage/0.1 (+https://github.com/bdefore/protondb-data)")

	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("query ProtonDB reports directory: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode > 299 {
		return nil, fmt.Errorf("query ProtonDB reports directory: GitHub API returned %s", resp.Status)
	}

	var items []githubContentItem
	if err := json.NewDecoder(resp.Body).Decode(&items); err != nil {
		return nil, fmt.Errorf("decode GitHub contents response: %w", err)
	}

	snapshots := make([]Snapshot, 0, len(items))
	for _, item := range items {
		if item.Type != "file" && item.Type != "" {
			continue
		}
		date, ok := ParseSnapshotFilename(item.Name)
		if !ok {
			continue
		}
		url := item.DownloadURL
		if url == "" {
			url = ReportsRawBaseURL + "/" + item.Name
		}
		snapshots = append(snapshots, Snapshot{
			Filename: item.Name,
			Date:     date,
			URL:      url,
			Size:     item.Size,
		})
	}
	SortSnapshotsNewestFirst(snapshots)
	return snapshots, nil
}

// LatestSnapshotFromGitHub returns the newest ProtonDB report archive metadata.
func LatestSnapshotFromGitHub(ctx context.Context, client *http.Client) (Snapshot, error) {
	snapshots, err := ListSnapshotsFromGitHub(ctx, client)
	if err != nil {
		return Snapshot{}, err
	}
	if len(snapshots) == 0 {
		return Snapshot{}, errors.New("no reports_*.tar.gz snapshots found in ProtonDB reports directory")
	}
	return snapshots[0], nil
}
