# ProtonSage

ProtonSage is a local Linux desktop utility for Steam/Proton troubleshooting. It uses ProtonDB data as the primary evidence source, scans the user's Steam library read-only, compares reports against local hardware/OS context, and builds safe copy/export launch-option recommendations.

Current stack test:

- Go backend/core
- Wails v2 desktop shell
- React + Vite + Tailwind setup for the frontend
- Local shadcn-style component primitives/CSS for the first UI smoke test
- SQLite schema draft for the upcoming importer

## Safety status

The current PoC is read-only for Steam files.

- It reads `libraryfolders.vdf` and `appmanifest_*.acf`.
- It does **not** write Steam config.
- It does **not** call external AI providers.
- It does **not** download ProtonDB report archives by default; `latest-snapshot` only reads GitHub directory metadata.

Future Steam config writes must have explicit confirmation, exact preview, timestamped backup, and restore path.

## Requirements

- Go 1.24+
- Node.js + npm
- Wails v2 CLI for desktop builds:

```bash
go install github.com/wailsapp/wails/v2/cmd/wails@v2.12.0
```

On this machine Wails builds with WebKitGTK 4.1, so `wails.json` includes:

```json
"build:tags": "wails,webkit2_41"
```

If building on a distro that still provides `webkit2gtk-4.0` instead of `webkit2gtk-4.1`, remove `webkit2_41` from `wails.json` or install the matching WebKitGTK development package.

## Development commands

Core/CLI checks:

```bash
go fmt ./...
go test ./...
go run ./cmd/protonsage --help
go run ./cmd/protonsage latest-snapshot
go run ./cmd/protonsage system-profile
go run ./cmd/protonsage scan-steam --dry-run
```

Frontend checks:

```bash
cd frontend
npm install
npm run build
```

Wails desktop build:

```bash
wails build -clean
```

If `$GOPATH/bin` is not on `PATH`, use:

```bash
$(go env GOPATH)/bin/wails build -clean
```

## CLI commands

```text
protonsage version
protonsage latest-snapshot
protonsage system-profile
protonsage scan-steam --dry-run [--root /path/to/Steam]
```

Examples:

```bash
go run ./cmd/protonsage latest-snapshot
go run ./cmd/protonsage scan-steam --dry-run --root "$HOME/.local/share/Steam"
```

## Project layout

```text
cmd/protonsage/          CLI entry point
internal/app/            Application facade shared by CLI/Wails
internal/core/           Domain models
internal/protondb/       Latest snapshot resolver
internal/steam/          Steam paths, VDF parser, read-only library scan
internal/system/         Read-only local hardware/OS profile detection
internal/advisor/        Deterministic advisor placeholder
internal/storage/        SQLite schema draft
frontend/                Wails React/Vite/Tailwind frontend
```

## ProtonDB source/license

Primary source: <https://github.com/bdefore/protondb-data>

The ProtonDB export data is published under ODbL/DbCL terms. ProtonSage must preserve attribution/license notes in docs, imports, and exports.
