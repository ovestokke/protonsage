# ProtonSage TODO

Durable implementation backlog. Broad product/architecture direction lives in `PRODUCT_PLAN.md`; the current short agent handoff lives in `plan.md`.

## Phase 0 — Now: repository bootstrap

- [x] Use the current `plan.md` handoff to bootstrap the Go + Wails PoC skeleton.
- [x] Finalize Go module/package layout plus Wails frontend layout.
- [x] Create initial directory skeleton: `cmd/protonsage/`, `internal/app/`, `internal/core/`, `internal/storage/`, `internal/protondb/`, `internal/steam/`, `internal/system/`, `internal/advisor/`, `frontend/`, `testdata/`.
- [x] Add README with product scope, local development commands, ProtonDB data/license note, and Wails/Node requirements.
- [x] Add formatting/test commands that work before full UI code exists: `go test ./...`, `go fmt ./...`, and frontend/Wails commands if initialized.
- [x] Define initial SQLite schema draft for games, reports, sources, launch options, and import metadata.
- [x] Define local system profile model for GPU/vendor/driver, CPU, RAM, distro, kernel, session/desktop, and relevant Steam/Proton context.
- [x] Define advisor output model shared by deterministic and optional AI recommendations.
- [x] Review ProtonForge only as a planning/reference source for Steam path detection, VDF parsing, checkbox-style launch-option building, preview/copy/write UX, and license implications.

## Phase 1 — Data PoC

- [x] Write a small latest-snapshot resolver for `https://github.com/bdefore/protondb-data` that lists `reports/reports_*.tar.gz`, parses dates from filenames, and selects the newest archive.
- [x] Confirm and document whether the selected modern archive is cumulative or incremental. Initial spot-check: May/June 2026 archives both contain one `reports_piiremoved.json` file and include records from 2019 through the snapshot date, so they appear cumulative for the modern schema.
- [x] Inspect only the latest ProtonDB data snapshot by default without cloning the full repository.
- [x] Write a small importer for one local/latest snapshot archive. Current verified path imports a local tiny `reports_*.tar.gz` fixture from `io.Reader`; large archive download/import remains later.
- [x] Store raw source identifiers, snapshot filename/date, source URL, import time, license note, report timestamps, app IDs/names, ratings, report text, launch options, Proton version, and ProtonDB report `systemInfo` metadata needed for ranking/similarity.
- [x] Add SQLite FTS for game names and report text.
- [x] Add import tests using a tiny fixture archive/sample.

## Phase 2 — Core recommendation flow / no-AI path

- [x] Implement game lookup by Steam appid and fuzzy/name search.
- [x] Implement report ranking where recency is a central scoring factor.
- [x] Normalize ProtonDB report `systemInfo` enough to compare GPU/vendor/driver, CPU class, RAM, distro, and kernel. Proton-version similarity remains later ranking work.
- [x] Add hardware/OS similarity as an explanatory ranking signal, secondary to recency.
- [x] Mark stale reports as historical context rather than current truth.
- [x] Extract launch option candidates from report text.
- [x] Turn extracted tips into selectable suggestion records with snippet, source reports, occurrence count, recency, hardware/OS similarity, confidence, and conflict notes.
- [x] Generate a deterministic recommendation summary with cited report/source references.
- [x] Build launch-option preview from selected checkbox/toggle suggestions.
- [x] Add tests for scoring, system-similarity behavior, stale-report behavior, launch-option extraction, and preview composition.

## Phase 3 — Steam read-only scan + native system detection — PoC required

- [x] Locate Steam installation and library folders on Linux, including common native paths and Flatpak Steam path.
- [x] Parse `steamapps/libraryfolders.vdf` read-only to discover all Steam library folders.
- [x] Parse `steamapps/appmanifest_*.acf` read-only for appid, name, install dir, size/state/build metadata.
- [x] Optionally read existing per-game launch options from `userdata/<user>/config/localconfig.vdf` read-only for display/context.
- [x] Match installed games to imported ProtonDB records.
- [x] Detect local system profile read-only: GPU/vendor/driver, CPU, RAM, distro, kernel, session/desktop, and relevant Steam/Proton context where feasible.
- [x] Normalize detected profile into the same comparison categories used for ProtonDB report `systemInfo`.
- [x] Expose scan and system profile results to core without UI dependencies.
- [x] Add Steam parser and system-detection fixtures/tests so the PoC does not depend on the developer's real library/machine for tests.

## Phase 4 — Wails UI shell

- [x] Create a minimal Wails desktop shell with a modern web UI direction.
- [x] Use React + Tailwind + shadcn/ui-style components unless a better Wails frontend choice is deliberately selected.
- [ ] Add installed-game list/search UI.
- [ ] Show lookup/ranking results through Go backend services only.
- [ ] Add a recommendation panel with freshness-weighted evidence and citations.
- [ ] Let the user choose which suggested tips/snippets to include using checkboxes/toggles.
- [ ] Show a live launch-option preview assembled from selected suggestions.
- [ ] Provide copy/export actions for launch options.
- [ ] Do not write Steam config in the PoC.

## Phase 5 — Optional AI advisor

- [ ] Define AI provider interface with local/offline provider preference and external provider opt-in.
- [ ] Define privacy disclosure for any external AI call: exact hardware/OS/library/report context being sent.
- [ ] Reuse native system profile rather than duplicating hardware/OS detection inside the AI layer.
- [ ] Build bounded AI context from selected game, ranked ProtonDB reports, report-vs-user system similarity, extracted snippets, stale-report warnings, existing launch options, and local hardware/OS.
- [ ] Require AI output to be structured: recommendation, exact launch options, rationale, confidence, risks, and citations.
- [ ] Add tests/fixtures for prompt assembly and output validation without calling a live provider.

## Later

- [ ] Compare multiple ProtonDB snapshots for trend changes over time.
- [ ] Add richer filters for GPU, distro, Proton version, and hardware class.
- [ ] Consider secondary sources only if clearly labeled and kept separate from ProtonDB-derived recommendations.
- [ ] Add optional Steam config write workflow only after explicit confirmation, exact change preview, timestamped backup, and restore design.
- [ ] If direct write is added, update `localconfig.vdf` via a real parser/serializer or a narrowly tested patcher; avoid broad regex rewrites of user config.
