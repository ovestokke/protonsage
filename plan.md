# ProtonSage Implementation Plan — Current Agent Handoff

This file is temporary and should describe only the next implementation slice. Durable product direction lives in `PRODUCT_PLAN.md`; backlog lives in `TODO.md`; agent rules live in `AGENTS.md`.

## Current state

The Go + Wails stack test has been bootstrapped.

Implemented:

- Go module and package layout.
- CLI at `cmd/protonsage`:
  - `version`
  - `latest-snapshot`
  - `system-profile`
  - `scan-steam --dry-run [--root ...]`
- Core models in `internal/core`.
- Application facade in `internal/app` shared by CLI/Wails.
- ProtonDB latest-snapshot resolver in `internal/protondb` using GitHub directory metadata only.
- Read-only Steam root detection, VDF parser, library scanner, and appmanifest parser in `internal/steam`.
- Read-only system profile detection/parsers in `internal/system`.
- Advisor placeholder in `internal/advisor`.
- SQLite schema draft in `internal/storage/schema.sql`.
- Wails root files behind build tag `wails`.
- React + Vite + Tailwind-enabled frontend with a polished Wails UI smoke test.
- `wails.json` uses `build:tags: "wails,webkit2_41"` for this system's WebKitGTK 4.1.
- README with development/build commands and ProtonDB license note.

Verified:

```bash
go test ./...
go test -tags 'wails,webkit2_41' ./...
go run ./cmd/protonsage --help
go run ./cmd/protonsage latest-snapshot
go run ./cmd/protonsage system-profile
go run ./cmd/protonsage scan-steam --dry-run
cd frontend && npm run build
$(go env GOPATH)/bin/wails build -clean
```

Latest observed ProtonDB snapshot remains `reports_jun1_2026.tar.gz`.

## Next objective

Implement the first real data/storage slice while preserving the current safety constraints:

1. Add a small SQLite storage package.
2. Add a tiny ProtonDB fixture importer for tests.
3. Add report/game search foundations.
4. Connect minimal imported-data status to CLI and Wails facade.

Do not implement full large archive download/import until the tiny fixture path and schema are tested.

## Constraints

- Do not write Steam config.
- Do not modify real Steam files.
- Do not make external AI calls.
- Do not clone `bdefore/protondb-data`.
- Do not download large ProtonDB archives by default.
- Keep domain logic independent of Wails/frontend code.
- Keep frontend file/system actions behind explicit Go backend methods.
- Keep implementation small and fixture-testable.

## Proposed next file additions

```text
internal/storage/
  db.go                    # open/init DB, apply schema.sql
  queries.go               # upsert/query games and reports
  db_test.go
internal/protondb/
  import.go                # import from io.Reader/tar.gz fixture first
  import_test.go
testdata/protondb/
  reports_sample.tar.gz or reports_piiremoved.json fixture
```

If embedding `schema.sql`, use Go `embed` inside `internal/storage` and keep SQL as the source of truth.

## Next implementation steps

### 1. Storage package

- Add `storage.Open(path string)` or `storage.OpenInMemory()`.
- Apply `schema.sql` idempotently.
- Add minimal insert/query functions for:
  - sources
  - import_runs
  - games
  - reports
  - report_system_info
- Keep SQLite dependency explicit and small. Candidate: `modernc.org/sqlite` for pure Go, or `github.com/mattn/go-sqlite3` if CGO is acceptable. Decide deliberately before adding.

### 2. Tiny ProtonDB fixture importer

- Create a tiny fixture matching the modern archive shape:
  - tar.gz containing `reports_piiremoved.json`
  - a handful of records with appid, timestamp, rating/verdict, notes, launch options, Proton version, and `systemInfo`.
- Import from an `io.Reader` so tests do not need network.
- Store import metadata: snapshot filename/date/source URL/import time/license note.

### 3. Query/search foundation

- Implement lookup by appid.
- Add simple name/text search first; FTS can come after the basic tests are green.
- Add tests proving imported reports are retrievable and ordered by timestamp descending.

### 4. CLI/app facade

Add CLI command only if storage/import is ready:

```bash
go run ./cmd/protonsage import-fixture --db /tmp/protonsage.db --fixture testdata/protondb/reports_sample.tar.gz
go run ./cmd/protonsage lookup --db /tmp/protonsage.db --appid 123
```

Expose a minimal Wails-safe method in `internal/app` such as `GetDataStatus()` after storage exists.

## Acceptance criteria for next slice

- `go test ./...` passes.
- Existing CLI commands keep working.
- A tiny ProtonDB fixture archive can be imported into SQLite.
- Imported reports can be queried by appid and sorted newest-first.
- No real ProtonDB archive is downloaded during tests.
- No Steam config write path is added.
- Wails/frontend still builds, or any UI build blocker is documented clearly.
