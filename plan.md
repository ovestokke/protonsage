# ProtonSage Implementation Plan — Current Agent Handoff

This file is temporary and should describe only the next implementation slice. Durable product direction lives in `PRODUCT_PLAN.md`; backlog lives in `TODO.md`; agent rules live in `AGENTS.md`.

## Current state

All phases 00-04 complete:

- Go + Wails skeleton, CLI, app facade, core models, Steam read-only scanner, system profile detection, latest ProtonDB snapshot resolver, frontend smoke UI.
- SQLite storage with `modernc.org/sqlite`, schema, import metadata, FTS5, CLI commands.
- ProtonDB snapshot importer handling real nested schema (`app.steam.appId`, `responses.verdict/notes/launchOptions/protonVersion`, Unix timestamps).
- Installed-game matching by appid then normalized name fallback.
- Deterministic no-AI advisor with recency-first ranking (72/20/8), 25 workaround patterns, suggestion grouping/deduping, conflict detection, quoted env-var preservation, cited summaries, launch-preview composition.
- **Phase 04 — Wails UI shell** complete.

## Phase 04 — Sluttrapport

### Backend additions

| Method | Go Binding | Description |
|--------|------------|-------------|
| `DbPath()` | `App.DbPath()` | Returns XDG data dir DB path (`$XDG_DATA_HOME/protonsage/protonsage.db`) |
| `GetDataStatus()` | `App.GetDataStatus()` | Report/game/import counts without explicit dbPath |
| `GetInstalledGames()` | `App.GetInstalledGames()` | Scan + match without explicit dbPath/root |
| `GetRecommendation(appid)` | `App.GetRecommendation(appid)` | Deterministic recommendation by appid |
| `SearchGames(query, limit)` | `App.SearchGames(query, limit)` | FTS5 game search |
| `BuildLaunchPreview(selected, existing)` | `App.BuildLaunchPreview(selected, existing)` | Compose launch options from selected suggestions |

All methods use `app.dbPath` internally (XDG auto-configured). The Go App struct stores `dbPath` as a field set at creation, not passed from frontend.

### Frontend screens/components

1. **Header**: ProtonSage branding, data status badge, system profile badge, safety badge, version.
2. **No-data bar**: Warning when no ProtonDB data imported, with import advice.
3. **Sidebar**: Installed game list with search/filter, report count badges, system profile summary.
4. **Recommendation panel**:
   - Game header with report count and freshness badges
   - Summary paragraph with warnings
   - Confidence strip (Freshness, System-aware, Cited)
   - Suggestion list with checkboxes, confidence badges, conflict warnings, show-more toggle
   - Launch option preview with copy button and safety badge
   - Existing launch options display
   - Evidence panel (toggle) showing top 5 ranked reports

5. **Empty/loading/error states**: No game selected, scanning, recommendation loading, no ProtonDB data.

### Design direction

- Dark, premium "compatibility cockpit" theme
- Grid background texture with radial gradient mask
- Cyan/green/amber accents on near-black panels
- JetBrains Mono for code snippets, Bahnschrift/Aptos Display for headers
- Badge system: freshness (fresh=green, recent=cyan, stale=amber, historical=muted), confidence (high/medium/low), report counts
- Safety badge on copy/export: "Copy / Export only — no Steam writes"

### Technical decisions

- **Local preview computation**: Frontend computes launch preview from selected suggestions without calling Go backend, for immediate UX. Backend `BuildLaunchPreview` remains available for exact verification.
- **Auto-select top 3 actionable suggestions** on game selection.
- **XDG data path**: `$XDG_DATA_HOME/protonsage/protonsage.db` with auto-creation of directory.
- **Generated Wails bindings**: `frontend/src/wailsjs/` auto-regenerated on `wails build -clean`.
- **Fallback/mock mode in wails.ts**: Browser preview mode returns stub data when `window.go` is unavailable.

### Build results

```
✓ go test ./... — all green
✓ cd frontend && npm run build — clean TypeScript + Vite build
✓ wails build -clean — builds in ~1.5s, binary at build/bin/protonsage
```

### Known gaps / next steps

1. **No game search in sidebar** — only filtering installed games. Future: add ProtonDB game search.
2. **No import-from-UI flow** — must use CLI to import data. Future: add snapshot download/import UI.
3. **No AI suggestions** — deterministic only. Phase 05.
4. **No Steam config writes** — Phase 07 (future, after confirmation UX).
5. **Pagination** — evidence panel shows top 5; full list not yet accessible.

## Next objective

Phase 05: Optional AI advisor boundary (later), or Phase 06: final integration/smoke test.

## Constraints

- Do not write Steam config.
- Do not modify real Steam files.
- Do not call external AI.
- Do not clone `bdefore/protondb-data`.
- Do not download large ProtonDB archives by default.
- Keep domain logic independent of Wails/frontend code.
- Keep frontend file/system actions behind explicit Go backend methods.
- Keep implementation small and fixture-testable.