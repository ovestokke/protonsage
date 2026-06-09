# Phase 01 review

PASS

## Review
- Correct: SQLite init/schema matches the Phase 01 scope. `storage.Open` trims/validates the DB path and applies embedded `schema.sql` on open (`internal/storage/db.go:35-70`); imports run through `WithTx` (`internal/storage/db.go:87-105`). The schema stores ProtonDB source/import metadata including snapshot filename/date/source URL/license, report rows, normalized/raw report system info, and FTS5 tables/triggers for game/report search (`internal/storage/schema.sql:5-23`, `internal/storage/schema.sql:32-61`, `internal/storage/schema.sql:75-113`).
- Correct: Query behavior covers the required operations. Sources and games are upserted, import runs are created/finished, reports and system info are inserted, lookup by appid works, reports are sorted newest-first by timestamp, and FTS search helpers exist (`internal/storage/queries.go:94-188`, `internal/storage/queries.go:195-290`, `internal/storage/queries.go:296-391`).
- Correct: Import behavior is local/fixture-safe. `ImportSnapshot` accepts an `io.Reader`, reads `reports_piiremoved.json` from a tar.gz stream, normalizes records, skips invalid records by count, stores metadata/attribution, and writes all DB changes inside one transaction (`internal/protondb/import.go:67-176`, `internal/protondb/import.go:179-234`, `internal/protondb/import.go:331-399`). Timestamp parsing handles strings and Unix seconds/millis without panics (`internal/protondb/import.go:442-505`).
- Correct: Fixture and tests meet the no-network/no-large-archive requirement. The checked fixture is tiny (905-byte tar.gz; 2,123-byte JSON) and contains `reports_piiremoved.json` with 2 appids and 3 reports. Tests open only the local fixture (`internal/protondb/import_test.go:21-32`) and verify counts, metadata/license/source URL, game lookup, newest-first ordering, GPU/system info, launch options, and FTS (`internal/protondb/import_test.go:36-97`; `internal/storage/db_test.go:12-133`). Grep found no test references to `LatestSnapshotFromGitHub`, `ListSnapshotsFromGitHub`, or HTTP clients.
- Correct: CLI behavior is usable and graceful for the Phase 01 commands. `import-fixture` requires `--db` and `--fixture`, opens only the explicit fixture and DB path, and passes metadata/license into the importer (`cmd/protonsage/main.go:71-123`). `lookup` requires `--db` and positive `--appid`, returns a clear not-found error, and prints game/reports JSON (`cmd/protonsage/main.go:126-163`). `data-status` requires `--db` and returns counts/latest import through the app facade (`cmd/protonsage/main.go:165-179`; `internal/app/service.go:62-80`).
- Correct: Safety constraints are preserved. `latest-snapshot` only calls the GitHub contents API and explicitly reports failures as non-archive-download failures (`cmd/protonsage/main.go:54-68`; `internal/protondb/snapshots.go:110-160`). Grep found no production Steam write path or AI provider calls. The only Steam writes found are fixture writes in `internal/steam/library_test.go`. The scan command rejects `--dry-run=false` (`cmd/protonsage/main.go:197-207`).
- Correct: Dependency direction is preserved. `internal/protondb` imports only `internal/core` and `internal/storage` (`internal/protondb/import.go:20-21`); `internal/storage` imports `internal/core` only (`internal/storage/queries.go:12`); grep found no Wails/frontend imports under `internal/`. The Wails layer remains thin and only delegates to `internal/app` (`app.go:12-31`).
- Correct: Docs/TODO/plan reflect actual status. README documents the pure-Go `modernc.org/sqlite` choice and no-archive/no-AI/no-Steam-write safety status (`README.md:13-21`, `README.md:118-120`). TODO marks Phase 1 done while leaving ranking/similarity/extraction in later phases (`TODO.md:17-38`). `plan.md` now points at the next non-AI recommendation slice and repeats safety constraints (`plan.md:40-59`).

## Blockers
- None. Phase 01 is acceptable based on inspected code and validation commands.

## Optional follow-ups
- Before importing real cumulative snapshots repeatedly, decide on report de-duplication or latest-import scoping. The current schema has a non-unique `(source_id, source_report_id)` index and `ReportsByAppID` reads all import runs (`internal/storage/schema.sql:48-49`; `internal/storage/queries.go:318-322`), so repeated imports of the same snapshot will duplicate visible reports.
- Add targeted tests for malformed timestamps/missing optional fields. The code skips invalid records (`internal/protondb/import.go:341-348`), but current fixture tests cover the happy path only.
- Consider a read-only open path for `lookup`/`data-status` so a typo in `--db` does not create a fresh empty SQLite DB. They currently call `storage.Open`, which creates/applies schema (`internal/storage/db.go:35-70`; `cmd/protonsage/main.go:141`; `internal/app/service.go:62-64`).
- For future full-archive imports, replace or bound the current `io.ReadAll` of `reports_piiremoved.json` (`internal/protondb/import.go:228`) if memory becomes an issue. It is fine for the Phase 01 tiny fixture.
- `progress.md` was requested but is absent in this worktree; review proceeded from `plan.md`, worker report, prompts, docs, and current diff.

## Commands run
```bash
git status --short && git diff --stat && git diff -- . ':!subagent-results/*'
gofmt -l cmd/protonsage internal app.go main.go
go test ./...
go test -tags 'wails,webkit2_41' ./...
git diff --check -- . ':!subagent-results/*'
tar -tzf testdata/protondb/reports_sample.tar.gz
wc -c testdata/protondb/reports_sample.tar.gz testdata/protondb/reports_piiremoved.json
go run ./cmd/protonsage import-fixture --db /tmp/protonsage-phase01-review.8aPlIN.db --fixture testdata/protondb/reports_sample.tar.gz
go run ./cmd/protonsage lookup --db /tmp/protonsage-phase01-review.8aPlIN.db --appid 123
go run ./cmd/protonsage data-status --db /tmp/protonsage-phase01-review.8aPlIN.db
go run ./cmd/protonsage lookup --db /tmp/protonsage-phase01-review.8aPlIN.db --appid 999
```

Validation highlights:
- `go test ./...`: passed.
- `go test -tags 'wails,webkit2_41' ./...`: passed.
- `gofmt -l ...`: no files listed.
- `git diff --check`: no whitespace errors.
- `import-fixture`: returned 2 games, 3 reports, 3 system-info rows, 0 skipped.
- `lookup --appid 123`: returned `r-123-new` before `r-123-old`.
- `data-status`: returned 1 source, 1 import run, 2 games, 3 reports, with ODbL/DbCL license metadata.
- `lookup --appid 999`: exited non-zero with `appid 999 was not found in imported data`.
