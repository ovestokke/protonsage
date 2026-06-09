# ProtonSage

ProtonSage is a local Linux desktop utility for Steam/Proton troubleshooting. It uses ProtonDB data as the primary evidence source, scans the user's Steam library read-only, compares reports against local hardware/OS context, and builds safe copy/export launch-option recommendations.

Current stack test:

- Go backend/core
- Wails v2 desktop shell
- React + Vite + Tailwind setup for the frontend
- Local shadcn-style component primitives/CSS for the first UI smoke test
- SQLite storage/import foundation using a tiny local ProtonDB fixture

## Safety status

The current PoC is read-only for Steam files.

- It reads `libraryfolders.vdf`, `appmanifest_*.acf`, and per-user `localconfig.vdf` launch options.
- It does **not** write Steam config.
- It does **not** call external AI providers.
- It does **not** download ProtonDB report archives by default; `latest-snapshot` only reads GitHub directory metadata.
- Fixture import writes only to the explicit SQLite DB path passed with `--db`.

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
go run ./cmd/protonsage import-fixture --db /tmp/protonsage-fixture.db --fixture testdata/protondb/reports_sample.tar.gz
go run ./cmd/protonsage lookup --db /tmp/protonsage-fixture.db --appid 123
go run ./cmd/protonsage data-status --db /tmp/protonsage-fixture.db
go run ./cmd/protonsage system-profile
go run ./cmd/protonsage scan-steam --dry-run
go run ./cmd/protonsage scan-steam --dry-run --root testdata/steam/native-root
go run ./cmd/protonsage installed --db /tmp/protonsage-fixture.db --root testdata/steam/native-root
go run ./cmd/protonsage recommend --db /tmp/protonsage-fixture.db --appid 123
go run ./cmd/protonsage launch-preview --db /tmp/protonsage-fixture.db --appid 123 --select <suggestion-id>
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
protonsage import-fixture --db /path/to/protonsage.db --fixture testdata/protondb/reports_sample.tar.gz
protonsage lookup --db /path/to/protonsage.db --appid 123
protonsage data-status --db /path/to/protonsage.db
protonsage system-profile
protonsage scan-steam --dry-run [--root /path/to/Steam]
protonsage installed --db /path/to/protonsage.db [--root /path/to/Steam]
protonsage recommend --db /path/to/protonsage.db --appid 123
protonsage launch-preview --db /path/to/protonsage.db --appid 123 --select <suggestion-id>[,<suggestion-id>] [--existing "..."]
```

Examples:

```bash
go run ./cmd/protonsage latest-snapshot
go run ./cmd/protonsage import-fixture --db /tmp/protonsage-fixture.db --fixture testdata/protondb/reports_sample.tar.gz
go run ./cmd/protonsage lookup --db /tmp/protonsage-fixture.db --appid 123
go run ./cmd/protonsage scan-steam --dry-run --root "$HOME/.local/share/Steam"
go run ./cmd/protonsage installed --db /tmp/protonsage-fixture.db --root testdata/steam/native-root
go run ./cmd/protonsage recommend --db /tmp/protonsage-fixture.db --appid 123
# Copy a suggestion id from recommend output, then:
go run ./cmd/protonsage launch-preview --db /tmp/protonsage-fixture.db --appid 123 --select <launch-suggestion-id>
```

## Project layout

```text
cmd/protonsage/          CLI entry point
internal/app/            Application facade shared by CLI/Wails
internal/core/           Domain models
internal/protondb/       Latest snapshot resolver and local tar.gz fixture importer
internal/steam/          Steam paths, VDF parser, read-only library scan and launch-options context
internal/system/         Read-only local hardware/OS profile detection and normalization
internal/advisor/        Deterministic no-AI ranking, extraction, recommendation, and preview logic
internal/storage/        SQLite schema, FTS tables, and concrete query helpers
frontend/                Wails React/Vite/Tailwind frontend
```

## ProtonDB source/license

Primary source: <https://github.com/bdefore/protondb-data>

The ProtonDB export data is published under ODbL/DbCL terms. ProtonSage stores snapshot filename/date/source URL/import time and an ODbL/DbCL attribution note in import metadata; exports must preserve that attribution.

SQLite driver decision: ProtonSage uses `modernc.org/sqlite` for this PoC storage slice because it is pure Go and avoids CGO requirements for local desktop distribution. No ORM is used.
