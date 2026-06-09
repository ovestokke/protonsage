# Phase 03 Review — Deterministic Recommendations

Result: **FAIL** — one launch-option extraction/preview blocker should be fixed before accepting Phase 03.

## Review

- **Correct:** Recency-first ranking is implemented and deterministic for fixed inputs. `internal/advisor/scoring.go:13-17` defines fresh/recent/historical thresholds, `internal/advisor/scoring.go:22-46` sharply lowers old report scores, and `internal/advisor/scoring.go:138-160` uses a stable score/timestamp/report-id sort with recency weighted at 72%, system similarity at 20%, and quality at 8%.
- **Correct:** Stale/historical reports are not filtered out and are marked as context. Ranked report reasons add “treat as historical context, not current truth” at `internal/advisor/scoring.go:243-254`, and recommendation warnings mark all/partial stale sets at `internal/advisor/recommendation.go:82-105`.
- **Correct:** System similarity uses normalized profiles and remains secondary to recency. The comparable profile model is in `internal/core/models.go:86-97`; similarity compares normalized GPU/CPU/RAM/distro/kernel/session fields with unknowns neutral at `internal/advisor/scoring.go:79-119`; ranking normalizes the local profile and report profile at `internal/advisor/scoring.go:123-140` and `internal/advisor/scoring.go:258-288`.
- **Correct:** Suggestions are cited and evidence-linked. `internal/core/models.go:99-140` carries citations and suggestion scoring metadata; `internal/advisor/extraction.go:45-94` groups suggestions and preserves source citations; top-level recommendation citations are collected at `internal/advisor/recommendation.go:117-145`.
- **Correct:** Preview composition is copy/export-only and skips non-launch note/workaround/diagnostic suggestions. `internal/advisor/preview.go:11-13` documents no Steam writes, `internal/advisor/preview.go:40-49` skips non-copyable/dangerous suggestions, and `internal/advisor/preview.go:79-83` emits exactly one `%command%` in normal paths.
- **Correct:** CLI/app-service exposure is usable. `internal/app/service.go:167-173` exposes recommendation/preview through the app facade; `cmd/protonsage/main.go:249-307` implements `recommend` and `launch-preview` with validation and JSON output.
- **Correct:** Safety/dependency boundaries look clean. I found no Wails/frontend imports in `internal/core`, `internal/advisor`, `internal/storage`, `internal/protondb`, `internal/system`, or `internal/steam`. ProtonDB snapshot lookup only reads GitHub directory metadata and explicitly does not download archives (`internal/protondb/snapshots.go:110-160`). No AI/cloud provider path or Steam config write path was found in the Phase 03 implementation.
- **Correct:** Tests cover the main requested behavior: recency/stale labels (`internal/advisor/advisor_test.go:11-42`), similarity (`internal/advisor/advisor_test.go:44-76`), recency beating an old exact match (`internal/advisor/advisor_test.go:78-109`), extraction/grouping/conflicts/citations (`internal/advisor/advisor_test.go:111-135`, `158-186`), preview composition (`internal/advisor/advisor_test.go:137-156`), and app-service recommendation/preview (`internal/app/service_test.go:43-72`).

- **Fixed:** None. This was a read-only validation pass; no source files were modified.

- **Blocker:** Quoted environment-variable launch options are corrupted, so extraction is not preserving exact cited snippets and preview can become invalid. `envAssignPattern` explicitly accepts quoted values at `internal/advisor/extraction.go:16`, but `cleanSnippet` and `trimToken` strip quote characters from token ends at `internal/advisor/extraction.go:262-269`; `extractCommandSnippets` applies `trimToken` while rebuilding `%command%` lines at `internal/advisor/extraction.go:141-165`, and `extractEnvAssignments` applies `cleanSnippet` at `internal/advisor/extraction.go:174-183`. `BuildLaunchPreview` also tokenizes through `cleanedFields`/`splitLaunchTokens` at `internal/advisor/preview.go:52-63` and `internal/advisor/preview.go:98-130`, so existing or selected quoted env vars can be mangled. Reproduction with a temporary fixture containing `Use WINEDLLOVERRIDES="dxgi=n,b" %command%` returned suggestions `WINEDLLOVERRIDES="dxgi=n,b %command%` and `WINEDLLOVERRIDES="dxgi=n,b` (missing the closing quote). This violates the Phase 03 “keep exact snippet” requirement and can produce a broken launch preview for common Proton/Wine overrides. Fix quote-preserving cleanup/tokenization and add tests for quoted `WINEDLLOVERRIDES=... %command%` extraction plus preview.

- **Note:** `progress.md` was requested but is not present in the worktree. `plan.md` and the worker report were read instead.
- **Note:** Docs mostly reflect the implemented state, but `TODO.md` phase numbering does not match `PRODUCT_PLAN.md`/prompt numbering for Steam/system vs deterministic recommendations. This is a clarity follow-up, not an acceptance blocker.

## Commands run

- `go test ./...` — passed.
- `go test -tags 'wails,webkit2_41' ./...` — passed.
- `gofmt -l internal/advisor internal/app app.go cmd/protonsage internal/core` — no files reported.
- `grep -R "github.com/wailsapp\|frontend\|wails" -n internal/core internal/advisor internal/storage internal/protondb internal/system internal/steam` — no forbidden imports found.
- Fixture CLI smoke:
  - `go run ./cmd/protonsage import-fixture --db /tmp/protonsage-review-*/fixture.db --fixture testdata/protondb/reports_sample.tar.gz` — passed, imported 2 games / 3 reports.
  - `go run ./cmd/protonsage data-status --db /tmp/protonsage-review-*/fixture.db` — passed.
  - `go run ./cmd/protonsage installed --db /tmp/protonsage-review-*/fixture.db --root testdata/steam/native-root` — passed.
  - `go run ./cmd/protonsage recommend --db /tmp/protonsage-review-*/fixture.db --appid 123` — passed; showed fresh report first and historical warning for old report.
  - `go run ./cmd/protonsage launch-preview --db /tmp/protonsage-review-*/fixture.db --appid 123 --select launch-258e809a --existing "-novid"` — passed; preview `RADV_PERFTEST=gpl %command% -novid`.
  - `go run ./cmd/protonsage launch-preview --db /tmp/protonsage-review-*/fixture.db --appid 123 --select work-cfc01ebb` — passed; workaround skipped with warning.
- Temporary quoted-env fixture reproduction — exposed the blocker above.
