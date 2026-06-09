# Phase 03 Implementation Report — Deterministic Recommendations

## Summary

Implemented the deterministic no-AI recommendation foundation for selected appids. The flow now queries imported reports, ranks evidence recency-first with system-similarity explanations, extracts cited launch-option/workaround suggestions, emits recommendation JSON, and composes copy-ready launch-option previews without writing Steam config.

Note: `/home/ove/projects/protonsage/context.md` was requested but is not present in the worktree; implementation proceeded from `plan.md`, `prompts/03-deterministic-recommendations.md`, and the project docs/code.

## Files changed

- `internal/core/models.go` — added suggestion constants, freshness labels, `SimilarityResult`, `RankedReport`, suggestion IDs, `PreviewResult`, recommendation ranked reports/warnings.
- `internal/advisor/models.go` — updated empty draft recommendation helper.
- `internal/advisor/scoring.go` — recency scoring, stale labels, system similarity, quality scoring, recency-first ranking.
- `internal/advisor/extraction.go` — launch option/env/wrapper/workaround extraction, grouping/deduping, citations, confidence, conflict notes, dangerous shell-token filtering.
- `internal/advisor/recommendation.go` — deterministic recommendation summary, warnings, citation aggregation.
- `internal/advisor/preview.go` — copy/export launch-option preview builder.
- `internal/advisor/advisor_test.go` — scoring, stale labeling, similarity, ranking, extraction, duplicate/conflict, preview, citation tests.
- `internal/app/service.go`, `internal/app/service_test.go` — app-service exposure/tests for recommendation and preview.
- `app.go` — thin Wails binding exposure for recommendation and preview.
- `cmd/protonsage/main.go` — `recommend` and `launch-preview` JSON CLI commands.
- `README.md`, `TODO.md`, `plan.md` — documented new commands/current phase state.

## Ranking/scoring rules

- Freshness labels: `fresh` < 90 days, `recent` < 365 days, `stale` >= 365 days, `historical` >= 730 days.
- Recency score strongly favors fresh reports and drops sharply after one year; historical reports bottom out at `0.05` but remain visible/cited as context.
- Final rank score is recency-first: `72% recency + 20% normalized system similarity + 8% quality`.
- System similarity compares normalized GPU vendor/model/driver, distro family, kernel, RAM bucket, session type, CPU vendor/class; unknown fields are explanatory/neutral.
- Quality is a small secondary signal from ProtonDB rating plus concrete launch-option/tweak/verdict presence.

## Extraction/suggestion rules

- Extracts explicit `launchOptions` first.
- Extracts safe `%command%` snippets from report notes/verdict text.
- Extracts known Proton/graphics env vars such as `PROTON_*`, `DXVK_*`, `VKD3D_*`, `RADV_*`, `MESA_*`, `WINEDLLOVERRIDES`, `MANGOHUD`, etc.
- Extracts wrapper commands including `gamemoderun`, `mangohud`, `gamescope`, `prime-run`, and `obs-gamecapture`.
- Extracts conservative note/workaround/diagnostic phrases such as `use Proton Experimental`, intro-video disabling/skipping, `black screen`, and `no tweaks required`.
- Groups duplicates/case variants by kind + canonical snippet while preserving cited source reports/snippets.
- Adds rule-based confidence (`high`/`medium`/`low`) from occurrence count, recency, similarity, and concrete/copyable kind.
- Adds conflict notes when selected/report suggestions set the same env var to multiple values.
- Filters obvious dangerous/destructive shell snippets with shell-control tokens or commands like `sudo`, `rm`, `curl`, `bash`, etc.

## Preview rules

- `BuildLaunchPreview(selected, existing)` never writes Steam config.
- Only `launch_option`, `env_var`, and `wrapper` suggestions are applied automatically.
- `workaround`, `diagnostic`, and `note` suggestions are skipped with warnings.
- Preview contains `%command%` exactly once.
- Env vars/wrappers are placed before `%command%`; existing launch options are preserved as prefix/suffix context depending on whether they already contain `%command%`.
- Duplicate tokens are deduped and env-var conflicts are returned as warnings/conflicts.

## CLI/API added

- `go run ./cmd/protonsage recommend --db /tmp/protonsage-fixture.db --appid 123`
- `go run ./cmd/protonsage launch-preview --db /tmp/protonsage-fixture.db --appid 123 --select <suggestion-id> [--existing "..."]`
- `internal/app.Service.GetRecommendation(ctx, dbPath, appid)`
- `internal/app.Service.BuildLaunchPreview(ctx, selected, existing)`
- Wails binding methods: `GetRecommendation`, `BuildLaunchPreview`

## Validation

Passed:

```bash
go fmt ./...
go test ./...
go test -tags 'wails,webkit2_41' ./...
go run ./cmd/protonsage --help
rm -f /tmp/protonsage-fixture.db
go run ./cmd/protonsage import-fixture --db /tmp/protonsage-fixture.db --fixture testdata/protondb/reports_sample.tar.gz
go run ./cmd/protonsage recommend --db /tmp/protonsage-fixture.db --appid 123
go run ./cmd/protonsage launch-preview --db /tmp/protonsage-fixture.db --appid 123 --select <fixture launch suggestion id>
```

Smoke result included a cited `RADV_PERFTEST=gpl %command%` suggestion and launch preview `RADV_PERFTEST=gpl %command%`.

## Blockers / risks

- No implementation blocker. Requested `context.md` is missing.
- Extraction is intentionally conservative; complex shell quoting, long `gamescope` command lines, and game-specific non-launch-option workarounds will need broader patterns later.
- Similarity is coarse and explanatory; Proton-version similarity remains future work.

## Recommended next phase

Phase 04: build the Wails/React recommendation UI around the new backend methods: installed-game/search selection, ranked evidence/citations, suggestion checkboxes, conflict/stale warnings, and live copy-only launch preview.
