# ProtonSage Implementation Plan — Current Agent Handoff

This file is temporary and should describe only the next implementation slice. Durable product direction lives in `PRODUCT_PLAN.md`; backlog lives in `TODO.md`; agent rules live in `AGENTS.md`.

## Current state

Implemented and verified:

- Go + Wails skeleton, CLI, app facade, core models, Steam read-only scanner, system profile detection, latest ProtonDB snapshot resolver, and frontend smoke UI from the bootstrap slice.
- SQLite storage package in `internal/storage` using `database/sql` + pure-Go `modernc.org/sqlite`.
- Idempotent `schema.sql` application with tables for sources, import runs, games, reports, report system info, launch-option suggestions, and FTS5 tables for game/report search.
- Local ProtonDB snapshot importer in `internal/protondb` that reads a `reports_*.tar.gz` stream containing `reports_piiremoved.json`, stores import metadata/attribution, upserts games, inserts reports, and stores normalized/raw `systemInfo`.
- Tiny fixture at `testdata/protondb/reports_sample.tar.gz` with 2 appids and 3 reports.
- CLI commands:
  - `version`
  - `latest-snapshot`
  - `import-fixture --db ... --fixture ...`
  - `lookup --db ... --appid ...`
  - `data-status --db ...`
  - `system-profile`
  - `scan-steam --dry-run [--root ...]`
  - `installed --db ... [--root ...]`
  - `recommend --db ... --appid ...`
  - `launch-preview --db ... --appid ... --select ...`
- Minimal imported-data status exposed through `internal/app.GetDataStatus`.
- Normalized system-profile categories in `internal/core` shared by local detection and ProtonDB report system info: GPU vendor/model/driver, CPU vendor/class, RAM bucket, distro family, kernel, and session type.
- Steam scan now also reads existing per-user `localconfig.vdf` launch options read-only and attaches them to installed games.
- Installed-game matching against imported data is exposed through `internal/app.GetInstalledGames` and the `installed` CLI command, using appid first and exact normalized name fallback.
- Deterministic no-AI advisor foundation in `internal/advisor`: recency-first report ranking, fresh/recent/stale/historical labels, normalized system similarity explanations, quality/tweak signals, launch-option/workaround extraction, suggestion grouping/deduping, simple env-var conflict notes, cited summaries, and copy/export launch preview composition.
- Recommendation and preview flows exposed through `internal/app.GetRecommendation`, `internal/app.BuildLaunchPreview`, Wails bindings, and CLI JSON commands.

Verified in this slice:

```bash
go fmt ./...
go test ./...
go test -tags 'wails,webkit2_41' ./...
go run ./cmd/protonsage --help
go run ./cmd/protonsage latest-snapshot
go run ./cmd/protonsage import-fixture --db /tmp/protonsage-fixture.db --fixture testdata/protondb/reports_sample.tar.gz
go run ./cmd/protonsage lookup --db /tmp/protonsage-fixture.db --appid 123
go run ./cmd/protonsage data-status --db /tmp/protonsage-fixture.db
go run ./cmd/protonsage system-profile
go run ./cmd/protonsage scan-steam --dry-run
go run ./cmd/protonsage installed --db /tmp/protonsage-fixture.db --root testdata/steam/native-root
go run ./cmd/protonsage recommend --db /tmp/protonsage-fixture.db --appid 123
go run ./cmd/protonsage launch-preview --db /tmp/protonsage-fixture.db --appid 123 --select <suggestion-id>
cd frontend && npm run build
```

## Next objective

Build the first recommendation UI slice without expanding safety scope:

1. Add an installed-game/search view that calls Go backend services only.
2. Show recommendation JSON for a selected appid: ranked evidence, citations, freshness/stale labels, system-similarity notes, suggestions, confidence, and conflict notes.
3. Let the user select copyable suggestions with checkboxes/toggles.
4. Show a live launch-option preview assembled through the Go app service.
5. Keep the PoC copy/export only; do not write Steam config or call AI.

## Constraints

- Do not write Steam config.
- Do not modify real Steam files.
- Do not make external AI calls.
- Do not clone `bdefore/protondb-data`.
- Do not download large ProtonDB archives by default.
- Keep domain logic independent of Wails/frontend code.
- Keep frontend file/system actions behind explicit Go backend methods.
- Keep implementation small and fixture-testable.
