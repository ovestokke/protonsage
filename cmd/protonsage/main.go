package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"time"

	"protonsage/internal/app"
	"protonsage/internal/core"
	"protonsage/internal/protondb"
	"protonsage/internal/steam"
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
	case "system-profile":
		return printJSON(svc.GetSystemProfile(context.Background()))
	case "scan-steam":
		return scanSteam(args[1:])
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

func scanSteam(args []string) error {
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

	roots := []string{}
	if *root != "" {
		roots = append(roots, *root)
	} else {
		roots = steam.ExistingRoots()
	}
	if len(roots) == 0 {
		fmt.Println("No Steam roots found. Use --root /path/to/Steam to scan an explicit read-only root.")
		return nil
	}

	allGames := []core.Game{}
	for _, scanRoot := range roots {
		games, err := steam.ScanRoot(scanRoot)
		if err != nil {
			return fmt.Errorf("scan %s: %w", scanRoot, err)
		}
		for _, game := range games {
			allGames = append(allGames, game)
		}
	}
	return printJSON(allGames)
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
  system-profile      Print read-only local hardware/OS profile as JSON
  scan-steam          Scan Steam libraries read-only and print installed games as JSON

Examples:
  go run ./cmd/protonsage --help
  go run ./cmd/protonsage latest-snapshot
  go run ./cmd/protonsage system-profile
  go run ./cmd/protonsage scan-steam --dry-run
  go run ./cmd/protonsage scan-steam --dry-run --root "$HOME/.local/share/Steam"

Safety:
  The PoC only reads Steam files. It does not write Steam config.
`)
}
