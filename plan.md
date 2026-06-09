# ProtonSage Implementation Plan — Current Agent Handoff

This file is temporary and should describe only the next implementation slice. Durable product direction lives in `PRODUCT_PLAN.md`; backlog lives in `TODO.md`; agent rules live in `AGENTS.md`.

## Current state

Implemented and verified:

- Go + Wails skeleton, CLI, app facade, core models, Steam read-only scanner, system profile detection, latest ProtonDB snapshot resolver, and frontend smoke UI from the bootstrap slice.
- SQLite storage package in `internal/storage` using `database/sql` + pure-Go `modernc.org/sqlite`.
- Idempotent `schema.sql` application with tables for sources, import runs, games, reports, report system info, launch-option suggestions, and FTS5 tables for game/report search.
- Local ProtonDB snapshot importer in `internal/protondb` that reads a `reports_*.tar.gz` stream containing `reports_piiremoved.json`, stores import metadata/attribution, upserts games, inserts reports, and stores normalized/raw `systemInfo`.
- **Real ProtonDB schema support**: Importer now handles both the original fixture schema and the real `protondb-data` schema with nested `app.steam.appId`, `app.title`, `responses.verdict`, `responses.notes`, `responses.launchOptions`, `responses.protonVersion`, Unix timestamps, and nested `systemInfo`.
- Tiny fixture at `testdata/protondb/reports_sample.tar.gz` with 2 appids and 3 reports.
- Real ProtonDB import verified: 371,588 reports, 32,526 games imported from `reports_jun1_2026.tar.gz` with 0 records skipped.
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
- **Phase 03 complete: ranking, extraction, suggestions, recommendation summary, and preview composition all implemented and tested.**

## Phase 03 — Deterministic recommendations: Sluttrapport

### Ranking/scoring-regler

| Komponent | Vekt | Implementasjon |
|-----------|------|---------------|
| Recency | 72% | `RecencyScore()`: linear decay med skarpt fall etter 1 år, floor 0.05 |
| System similarity | 20% | `SystemSimilarity()`: 9 felt med ulik vekt (GPU vendor 0.28, distro 0.15, GPU model 0.12, etc.), partial match for GPU model, unknown fields explanatory |
| Report quality/confidence | 8% | `ReportQualityScore()`: rating (platinum→borked), concrete tweak bonus, verdict bonus |

Freshness thresholds: fresh <90d, recent <365d, stale <730d, historical ≥730d. Stale/historical merkes tydelig i summary og warnings.

### Extraction-regler

1. **Eksplisitt launchOptions-felt** → `launch_option` kind
2. **%command% lines** → `launch_option`, med wrapper/env-var tokens før og game args etter
3. **Env assignments** → `env_var` kind, kun kjente PROTON_/DXVK_/VKD3D_/MESA_/WINE/__/NVIDIA_/AMD_/SDL_ prefixes + WINEDLLOVERRIDES/MANGOHUD/etc
4. **Wrapper commands** → `wrapper` kind (gamemoderun, mangohud, gamescope, prime-run, obs-gamecapture)
5. **Known workaround patterns** → 25 regex-mønstre for vanlige ProtonDB-råd (Proton GE, windowed mode, intro skip, anti-cheat, controller, black/white screen, etc.)
6. **Dangerous token filtering**: shellkontrolltegn, sudo/rm/pkexec/etc blokkes
7. **Quoted env-var preservation**: `WINEDLLOVERRIDES="dxgi=n,b"` bevares intact
8. **Conflict detection**: env vars med samme navn men ulik verdi flagges

### CLI/API

| Kommando | Beskrivelse |
|----------|------------|
| `recommend --db ... --appid ...` | Genererer deterministisk recommendation JSON med ranked reports, suggestions, citations, warnings |
| `launch-preview --db ... --appid ... --select ... --existing ...` | Komponerer copy-ready launch options fra valgte suggestion-IDs |
| `GetRecommendation(ctx, dbPath, appid)` | App service binding |
| `BuildLaunchPreview(ctx, selected, existing)` | App service binding |

### Testresultater

```
ok  protonsage/internal/advisor    (all pass)
ok  protonsage/internal/app        (all pass)
ok  protonsage/internal/core       (all pass)
ok  protonsage/internal/protondb   (all pass, incl. real-schema test)
ok  protonsage/internal/steam      (all pass)
ok  protonsage/internal/storage    (all pass)
ok  protonsage/internal/system     (all pass)
```

Tests dekker:
- Recency score med faste `now` timestamps
- Stale/historical labeling
- System similarity exact/vendor/mismatch med partial GPU model match
- Fersk mismatch slår veldig gammel exact match (men exact match forklares)
- Extraction av %command%, env vars, wrappers
- Quoted env var preservation
- Duplicate grouping med occurrences og citations
- Conflict notes for motstridende env vars
- Preview composition med %command% kun én gang
- Existing launch options preservation
- Preview ordering (env vars før wrappers, existing før nye)
- Stale report warnings
- Empty reports → no-reports message
- Workaround pattern extraction (10 tester for 25 mønstre)

### Ekte data end-to-end

- Real ProtonDB import: 371,588 reports, 32,526 games, 0 skipped
- Factorio (appid 427520): 325 ranked reports, 26 suggestions, 76 citations
- Launch preview: `SDL_VIDEODRIVER=wayland gamemoderun %command% -novid` med existing launch options preserved
- 23 real Steam games scanned, 16 matched with ProtonDB data

### Kjente edge cases/TODOs

1. **Fritekst-pattern-matching** er begrensa til 25 kjente regex-mønstre. Mer avansert NLP-ekstraksjon er fremtidig arbeid.
2. **Proton-versjon matching** er ennå ikke en scoring-komponent (kun z-score bidrag).
3. **Navnesøk fallback** ved appid-mismatch kan forbedres med fuzzy search.
4. **Report notes** for store rapporter trimming pad til 180 tegn i citation snippets.

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