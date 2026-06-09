# Phase 02 implementation report

## Implemented

- Added comparable normalized system-profile categories in `internal/core` and populated them for local detection in `internal/system`.
  - GPU vendor/model, GPU driver major.minor, CPU vendor/class, RAM bucket, distro family, kernel major.minor, and session type.
  - Imported ProtonDB report system info is exposed with the same normalized categories in storage/query results while preserving raw JSON.
- Extended read-only Steam scanning to read existing per-user launch options from `userdata/*/config/localconfig.vdf`.
  - Supports the common `UserLocalConfigStore/Software/Valve/Steam/apps/<appid>/LaunchOptions` shape plus a recursive fixture-safe fallback.
  - Launch options are attached to `core.Game.ExistingLaunchOptions` as context only; no Steam config write path was added.
- Added installed-game matching against the imported SQLite data set.
  - Appid match is used first.
  - Exact normalized name fallback is used if appid is missing from imported data.
  - Results report installed status, match kind, matched ProtonDB appid/game, report count, and whether reports exist.
- Added CLI/app exposure.
  - `internal/app.ScanSteam(...)`
  - `internal/app.GetInstalledGames(...)`
  - Wails binding delegation for scan/matching.
  - New CLI: `installed --db /path/to/db [--root /path/to/Steam]`.
- Added Steam fixtures/tests for secondary library scan and `localconfig.vdf` launch options.

## Steam paths/formats supported

- Root candidates remain native and Flatpak Linux Steam paths:
  - `$HOME/.steam/steam`
  - `$HOME/.steam/root`
  - `$HOME/.local/share/Steam`
  - `$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam`
- Reads `steamapps/libraryfolders.vdf`, includes the root library, external library paths, symlink/dedup handling where practical, and deterministic sorting.
- Reads `steamapps/appmanifest_*.acf` for appid/name/install path/library path/size/state/build id.
- Reads `userdata/*/config/localconfig.vdf` for existing launch options as read-only context.

## Files changed/added for this phase

- `app.go`
- `cmd/protonsage/main.go`
- `internal/app/service.go`
- `internal/app/service_test.go`
- `internal/core/models.go`
- `internal/core/system_normalization.go`
- `internal/core/system_normalization_test.go`
- `internal/protondb/import_test.go`
- `internal/steam/library.go`
- `internal/steam/library_test.go`
- `internal/steam/localconfig.go`
- `internal/steam/localconfig_test.go`
- `internal/steam/paths.go`
- `internal/storage/queries.go`
- `internal/storage/db_test.go`
- `internal/system/profile.go`
- `internal/system/profile_test.go`
- `testdata/steam/appmanifest_456.acf`
- `testdata/steam/localconfig.vdf`
- `testdata/steam/native-root/...`
- `testdata/steam/secondary-library/...`
- `README.md`, `TODO.md`, `plan.md`

## Validation

Passed:

```bash
go fmt ./...
go test ./...
go test -tags 'wails,webkit2_41' ./...
go run ./cmd/protonsage --help
go run ./cmd/protonsage system-profile
go run ./cmd/protonsage scan-steam --dry-run
go run ./cmd/protonsage scan-steam --dry-run --root testdata/steam/native-root
go run ./cmd/protonsage import-fixture --db "$db" --fixture testdata/protondb/reports_sample.tar.gz
go run ./cmd/protonsage installed --db "$db" --root testdata/steam/native-root
cd frontend && npm run build
git diff --check
```

## Blockers / notes

- `context.md` was requested but is not present in the worktree; proceeded from `plan.md`, the phase prompt, docs, and current code.
- No Steam files were written. Test writes are limited to temp dirs/fixtures.
- No external AI calls and no ProtonDB archive downloads/clones were added.
- Known limitation: malformed app manifests still use the existing scan error behavior rather than a structured warning list. Optional malformed `localconfig.vdf` data is ignored for scan continuity.

## Recommended next phase

Proceed to deterministic recommendation foundations: recency-first report ranking, similarity scoring/explanations using the normalized profiles, launch-option/workaround extraction, cited suggestions, and launch-option preview composition.
