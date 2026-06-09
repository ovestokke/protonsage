package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"protonsage/internal/app"
	"protonsage/internal/core"
	"protonsage/internal/protondb"
	"protonsage/internal/steam"
	"protonsage/internal/storage"
)

func main() {
	if err := run(os.Args[1:]); err != nil {
		fmt.Fprintf(os.Stderr, "protonsage: %v\n", err)
		os.Exit(1)
	}
}

func run(args []string) error {
	if len(args) == 0 || args[0] == "--help" || args[0] == "-h" || args[0] == "help" {
		printUsage()
		return nil
	}

	svc := app.NewService()
	switch args[0] {
	case "version":
		return printJSON(svc.GetAppInfo())
	case "latest-snapshot":
		return latestSnapshot(args[1:])
	case "import-fixture":
		return importFixture(args[1:])
	case "lookup":
		return lookup(args[1:])
	case "data-status":
		return dataStatus(svc, args[1:])
	case "system-profile":
		return printJSON(svc.GetSystemProfile(context.Background()))
	case "scan-steam":
		return scanSteam(svc, args[1:])
	case "installed":
		return installed(svc, args[1:])
	case "recommend":
		return recommend(svc, args[1:])
	case "launch-preview":
		return launchPreview(svc, args[1:])
	default:
		printUsage()
		return fmt.Errorf("unknown command %q", args[0])
	}
}

func latestSnapshot(args []string) error {
	fs := flag.NewFlagSet("latest-snapshot", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	timeout := fs.Duration("timeout", 15*time.Second, "GitHub API timeout")
	if err := fs.Parse(args); err != nil {
		return err
	}

	ctx, cancel := context.WithTimeout(context.Background(), *timeout)
	defer cancel()
	snapshot, err := protondb.LatestSnapshotFromGitHub(ctx, nil)
	if err != nil {
		return fmt.Errorf("latest-snapshot failed without downloading archives: %w", err)
	}
	return printJSON(snapshot)
}

func importFixture(args []string) error {
	fs := flag.NewFlagSet("import-fixture", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	dbPath := fs.String("db", "", "SQLite DB file to create/update")
	fixturePath := fs.String("fixture", "", "local tiny ProtonDB reports_*.tar.gz fixture")
	snapshotDateText := fs.String("snapshot-date", "", "snapshot date as YYYY-MM-DD; defaults to date parsed from filename or fixture date")
	sourceURL := fs.String("source-url", "", "source URL to store in import metadata; defaults to file:// fixture path")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *dbPath == "" {
		return fmt.Errorf("import-fixture requires --db")
	}
	if *fixturePath == "" {
		return fmt.Errorf("import-fixture requires --fixture")
	}

	file, err := os.Open(*fixturePath)
	if err != nil {
		return fmt.Errorf("open fixture %s: %w", *fixturePath, err)
	}
	defer file.Close()

	db, err := storage.Open(*dbPath)
	if err != nil {
		return err
	}
	defer db.Close()

	snapshotFilename := filepath.Base(*fixturePath)
	snapshotDate, err := fixtureSnapshotDate(snapshotFilename, *snapshotDateText)
	if err != nil {
		return err
	}
	metadataURL := *sourceURL
	if metadataURL == "" {
		absPath, err := filepath.Abs(*fixturePath)
		if err != nil {
			return fmt.Errorf("resolve fixture path: %w", err)
		}
		metadataURL = "file://" + absPath
	}

	result, err := protondb.ImportSnapshot(context.Background(), db, file, protondb.SnapshotImportMeta{
		SnapshotFilename: snapshotFilename,
		SnapshotDate:     snapshotDate,
		SourceURL:        metadataURL,
		LicenseNote:      protondb.DataLicenseNote,
	})
	if err != nil {
		return err
	}
	return printJSON(result)
}

func lookup(args []string) error {
	fs := flag.NewFlagSet("lookup", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	dbPath := fs.String("db", "", "SQLite DB file to read")
	appid := fs.Int("appid", 0, "Steam appid to look up")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *dbPath == "" {
		return fmt.Errorf("lookup requires --db")
	}
	if *appid <= 0 {
		return fmt.Errorf("lookup requires --appid")
	}

	db, err := storage.Open(*dbPath)
	if err != nil {
		return err
	}
	defer db.Close()

	ctx := context.Background()
	game, ok, err := db.LookupGame(ctx, *appid)
	if err != nil {
		return err
	}
	if !ok {
		return fmt.Errorf("appid %d was not found in imported data", *appid)
	}
	reports, err := db.ReportsByAppID(ctx, *appid)
	if err != nil {
		return err
	}
	return printJSON(struct {
		Game    core.Game              `json:"game"`
		Reports []storage.ReportRecord `json:"reports"`
	}{Game: game, Reports: reports})
}

func dataStatus(svc *app.Service, args []string) error {
	fs := flag.NewFlagSet("data-status", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	dbPath := fs.String("db", "", "SQLite DB file to inspect")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *dbPath == "" {
		return fmt.Errorf("data-status requires --db")
	}
	status, err := svc.GetDataStatus(context.Background(), *dbPath)
	if err != nil {
		return err
	}
	return printJSON(status)
}

func fixtureSnapshotDate(snapshotFilename, explicit string) (time.Time, error) {
	if explicit != "" {
		date, err := time.Parse("2006-01-02", explicit)
		if err != nil {
			return time.Time{}, fmt.Errorf("parse --snapshot-date: %w", err)
		}
		return date, nil
	}
	if date, ok := protondb.ParseSnapshotFilename(snapshotFilename); ok {
		return date, nil
	}
	// The checked-in fixture is synthetic but models the latest observed PoC snapshot date.
	return time.Date(2026, time.June, 1, 0, 0, 0, 0, time.UTC), nil
}

func scanSteam(svc *app.Service, args []string) error {
	fs := flag.NewFlagSet("scan-steam", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	dryRun := fs.Bool("dry-run", true, "read-only scan mode; must remain true in the PoC")
	root := fs.String("root", "", "explicit Steam root to scan")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if !*dryRun {
		return fmt.Errorf("only --dry-run=true is supported; ProtonSage PoC never writes Steam config")
	}
	if *root == "" && len(steam.ExistingRoots()) == 0 {
		fmt.Println("No Steam roots found. Use --root /path/to/Steam to scan an explicit read-only root.")
		return nil
	}

	games, err := svc.ScanSteam(context.Background(), *root)
	if err != nil {
		return err
	}
	return printJSON(games)
}

func installed(svc *app.Service, args []string) error {
	fs := flag.NewFlagSet("installed", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	dbPath := fs.String("db", "", "SQLite DB file to match against installed games")
	root := fs.String("root", "", "explicit Steam root to scan")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *dbPath == "" {
		return fmt.Errorf("installed requires --db")
	}
	if *root == "" && len(steam.ExistingRoots()) == 0 {
		fmt.Println("No Steam roots found. Use --root /path/to/Steam to scan an explicit read-only root.")
		return nil
	}
	statuses, err := svc.GetInstalledGames(context.Background(), *dbPath, *root)
	if err != nil {
		return err
	}
	return printJSON(statuses)
}

func recommend(svc *app.Service, args []string) error {
	fs := flag.NewFlagSet("recommend", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	dbPath := fs.String("db", "", "SQLite DB file to read")
	appid := fs.Int("appid", 0, "Steam appid to recommend for")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *dbPath == "" {
		return fmt.Errorf("recommend requires --db")
	}
	if *appid <= 0 {
		return fmt.Errorf("recommend requires --appid")
	}
	recommendation, err := svc.GetRecommendation(context.Background(), *dbPath, *appid)
	if err != nil {
		return err
	}
	return printJSON(recommendation)
}

func launchPreview(svc *app.Service, args []string) error {
	fs := flag.NewFlagSet("launch-preview", flag.ContinueOnError)
	fs.SetOutput(os.Stderr)
	dbPath := fs.String("db", "", "SQLite DB file to read")
	appid := fs.Int("appid", 0, "Steam appid to recommend for")
	selectIDs := fs.String("select", "", "comma-separated suggestion IDs from recommend output")
	existing := fs.String("existing", "", "existing Steam launch options to preserve as preview context")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *dbPath == "" {
		return fmt.Errorf("launch-preview requires --db")
	}
	if *appid <= 0 {
		return fmt.Errorf("launch-preview requires --appid")
	}
	ids := parseSelectedIDs(*selectIDs)
	if len(ids) == 0 {
		return fmt.Errorf("launch-preview requires --select with one or more suggestion IDs")
	}

	recommendation, err := svc.GetRecommendation(context.Background(), *dbPath, *appid)
	if err != nil {
		return err
	}
	byID := map[string]core.Suggestion{}
	for _, suggestion := range recommendation.Suggestions {
		byID[suggestion.ID] = suggestion
	}
	selected := make([]core.Suggestion, 0, len(ids))
	for _, id := range ids {
		suggestion, ok := byID[id]
		if !ok {
			return fmt.Errorf("suggestion id %q was not found for appid %d", id, *appid)
		}
		selected = append(selected, suggestion)
	}
	return printJSON(svc.BuildLaunchPreview(context.Background(), selected, *existing))
}

func parseSelectedIDs(value string) []string {
	fields := strings.FieldsFunc(value, func(r rune) bool {
		return r == ',' || r == ' ' || r == '\n' || r == '\t'
	})
	ids := make([]string, 0, len(fields))
	seen := map[string]bool{}
	for _, field := range fields {
		field = strings.TrimSpace(field)
		if field == "" || seen[field] {
			continue
		}
		seen[field] = true
		ids = append(ids, field)
	}
	return ids
}

func printJSON(value any) error {
	encoder := json.NewEncoder(os.Stdout)
	encoder.SetIndent("", "  ")
	return encoder.Encode(value)
}

func printUsage() {
	fmt.Print(`ProtonSage local Steam/Proton advisor

Usage:
  protonsage <command> [options]

Commands:
  version             Print app/version/capability metadata as JSON
  latest-snapshot     Read GitHub metadata and print newest ProtonDB reports_*.tar.gz snapshot
  import-fixture      Import a local tiny ProtonDB tar.gz fixture into a selected SQLite DB
  lookup              Print imported game/report data for an appid
  data-status         Print imported-data counts/metadata for a selected SQLite DB
  system-profile      Print read-only local hardware/OS profile as JSON
  scan-steam          Scan Steam libraries read-only and print installed games as JSON
  installed           Scan installed Steam games and match them against imported ProtonDB data
  recommend           Generate deterministic cited recommendations for an appid
  launch-preview      Compose selected suggestion IDs into copy-ready launch options

Examples:
  go run ./cmd/protonsage --help
  go run ./cmd/protonsage latest-snapshot
  go run ./cmd/protonsage import-fixture --db /tmp/protonsage-fixture.db --fixture testdata/protondb/reports_sample.tar.gz
  go run ./cmd/protonsage lookup --db /tmp/protonsage-fixture.db --appid 123
  go run ./cmd/protonsage data-status --db /tmp/protonsage-fixture.db
  go run ./cmd/protonsage system-profile
  go run ./cmd/protonsage scan-steam --dry-run
  go run ./cmd/protonsage scan-steam --dry-run --root "$HOME/.local/share/Steam"
  go run ./cmd/protonsage installed --db /tmp/protonsage-fixture.db --root "$HOME/.local/share/Steam"
  go run ./cmd/protonsage recommend --db /tmp/protonsage-fixture.db --appid 123
  go run ./cmd/protonsage launch-preview --db /tmp/protonsage-fixture.db --appid 123 --select launch-...

Safety:
  The PoC only reads Steam files. It does not write Steam config.
`)
}
