# ProtonSage Product & Architecture Plan

This is the durable product plan. Keep broad ideas, architecture decisions, milestones, and future direction here. Use `plan.md` only as a temporary implementation handoff for the next agent/dev slice.

## Product summary

ProtonSage is a local Linux desktop utility that helps users interpret ProtonDB reports for their installed Steam games. It should favor fresh reports, surface launch options/workarounds, compare report authors' systems with the user's hardware/OS, and present recommendations with clear source references.

The intended direction is a sexy local desktop app built with a Go backend/core and Wails desktop shell. The frontend should use a modern web UI stack, currently React + Tailwind + shadcn/ui style components, so the app feels like a polished 2026 utility rather than a legacy desktop form. Users should be able to inspect ProtonDB-derived tips, choose which suggestions to use, and initially copy/paste launch options into Steam. Later versions may write Steam config directly, but only with explicit confirmation, exact preview, backup, and restore.

## Core product principles

- ProtonDB/protondb-data is the primary source.
- Latest ProtonDB snapshot only by default.
- Recency is central: stale reports are historical context, not current truth.
- Native Steam library scan is required for the PoC.
- Native hardware/OS detection is required for comparing the user's setup with ProtonDB report `systemInfo`.
- AI is optional but recommended; no core workflow may require AI.
- First PoC is copy/export only and must not write Steam config.

## Architecture

Current stack direction: Go backend/core + Wails desktop shell + modern web frontend.

Keep domain logic independent from Wails and frontend framework code. The frontend should be a thin presentation layer calling explicit Go backend methods exposed through Wails.

```text
go.mod / go.work
main.go, app.go          Wails GUI entry/bindings
wails.json               Wails configuration
cmd/protonsage/          CLI/dev entry point using the same internal packages
internal/app/            Application service facade used by CLI/Wails bindings
internal/core/           Domain models, scoring, recommendation logic, citation helpers
internal/storage/        SQLite schema, migrations, FTS indexes, query layer
internal/protondb/       Latest-snapshot resolver/import pipeline
internal/steam/          Steam path detection, VDF parsing, app manifest/library scan
internal/system/         Native hardware/OS detection and normalization
internal/advisor/        Rule-based advisor plus optional AI provider boundary
frontend/                React + Tailwind + shadcn/ui-style Wails frontend
tests/ or package tests  Unit/integration tests for non-UI modules
```

Suggested dependency direction:

```text
frontend -> Wails bindings -> internal/app -> internal/{advisor,core,storage,steam,system,protondb}
internal/advisor -> internal/{core,storage,steam,system}
internal/protondb -> internal/{core,storage}
internal/steam -> internal/core
internal/system -> internal/core
internal/core -> no Wails/frontend dependencies
cmd/protonsage -> internal packages for CLI/dev flows
```

## ProtonDB data plan

Source: https://github.com/bdefore/protondb-data

- Data archives live under `reports/reports_*.tar.gz`.
- Do not clone the full multi-GB repository for normal operation.
- A small resolver should list archive names through the GitHub contents API or equivalent, parse dates from filenames, and select the newest archive.
- Download/read only the latest archive by default.
- Current spot-check: May/June 2026 archives both contain `reports_piiremoved.json` and include records from 2019 through the snapshot date, so modern exports appear cumulative, not incremental.
- Import metadata should record snapshot filename/date, source URL, import time, and license attribution.
- Preserve ProtonDB report timestamps and `systemInfo` fields for freshness and hardware/OS similarity scoring.
- ProtonDB report data is ODbL/DbCL; docs and exports must preserve attribution/license notes.

## Steam/library plan

PoC must scan the user's Steam library read-only.

- Detect common native Steam roots and Flatpak Steam root.
- Parse `steamapps/libraryfolders.vdf` to discover all library folders.
- Parse `steamapps/appmanifest_*.acf` for appid, name, install dir, size/state/build metadata where available.
- Optionally read existing per-game launch options from `userdata/<user>/config/localconfig.vdf` read-only for display/context.
- Match installed games to imported ProtonDB records by appid first, name fallback only if needed.
- Direct writes to `localconfig.vdf` are out of PoC scope.

## Hardware/OS profile plan

Native detection should be available without AI.

Detect read-only where feasible:

- GPU vendor/model and driver version.
- CPU model/class.
- RAM.
- Distro/version.
- Kernel.
- Desktop/session type where relevant.
- Relevant Steam/Proton context when available.

Normalize local detection and ProtonDB report `systemInfo` into comparable categories. Use similarity as an explanatory ranking signal, secondary to recency.

Example UI language:

- "Fresh reports from similar NVIDIA driver/kernel."
- "Most suggested workaround reports are AMD users; treat as lower confidence for your NVIDIA setup."
- "Old report matches your GPU, but newer reports disagree; recency wins."

## Advisor design

Two paths share the same retrieved evidence.

### No-AI / deterministic mode

- Rank fresh reports.
- Extract explicit `launchOptions`, full `%command%` lines, environment variables, wrapper commands, and recurring workarounds.
- Compare report `systemInfo` against the user's local hardware/OS profile.
- Present suggestions as selectable checkboxes/toggles with occurrence counts, freshness, similarity, confidence, conflicts, and source reports.
- Build a launch-option preview from selected snippets for copy/export.

### Optional AI mode

- Assemble bounded context: game identity, detected hardware/OS, report-vs-user similarity, existing launch options, top fresh ProtonDB reports, extracted snippets, and stale-report warnings.
- Ask for a concise, grounded recommendation with citations.
- Output should be structured: recommendation, exact launch options, rationale, confidence, risks, and cited report IDs/snippets.
- Prefer local/offline providers where practical.
- External/cloud providers require explicit opt-in and a clear disclosure of exactly what data will be sent.

## ProtonForge reference ideas

Reference: https://github.com/theinvisible/proton-forge

Use as inspiration, not as architecture to clone blindly:

- Steam path detection for native and Flatpak Steam.
- VDF parsing for `libraryfolders.vdf`, `appmanifest_*.acf`, and `localconfig.vdf`.
- Read existing per-game launch options.
- Launch-option extraction/ranking: explicit fields first, `%command%` lines, known env vars, wrapper commands such as `gamemoderun`, `mangohud`, and `gamescope`.
- UI pattern: checkbox/toggle builder, live preview, copy first, direct write later.

If code is reused, preserve license attribution. Prefer fresh implementation of small parsers/extractors when practical.

## Milestones

### 0. Repo bootstrap

- Use Go + Wails as the test stack.
- Add Go module, Wails app skeleton, frontend scaffold, formatting/lint/test commands, and minimal checks.
- Add README with scope, development commands, ProtonDB license note, and Wails/Node requirements.
- Add core models for reports, games, system profile, suggestions, and recommendations.

### 1. Data schema and import

- Define SQLite schema for games, reports, sources, launch options, report system info, and import metadata.
- Implement latest-snapshot resolver.
- Import one local/latest ProtonDB snapshot.
- Add SQLite FTS for game lookup and report text search.
- Add import tests with tiny fixtures.

### 2. Steam scan and system profile

- Implement VDF parser.
- Scan Steam libraries read-only.
- Parse app manifests.
- Detect local hardware/OS profile read-only.
- Add fixtures/tests.

### 3. Search, ranking, and non-AI advisor

- Lookup by appid/name.
- Rank reports by recency and system similarity.
- Extract launch-option/workaround candidates.
- Generate deterministic, cited recommendations.
- Build launch-option preview from selected checkbox suggestions.

### 4. Wails UI shell

- Modern React/Tailwind/shadcn-style desktop UI through Wails.
- Installed-game list/search.
- Recommendation panel with freshness evidence, source citations, system-similarity notes, selectable snippets, and launch-option preview.
- Copy/export only for PoC.

### 5. Optional AI advisor

- Provider boundary.
- Prompt/context assembly.
- Structured output validation.
- Local/offline preference and external-provider opt-in.

### 6. Safe Steam config write

Later only:

- Exact diff/preview.
- Timestamped backup.
- Restore workflow.
- Parser/serializer or narrowly tested patcher; avoid broad regex rewrites of user config.

## PoC success criteria

- Newest ProtonDB archive can be identified and imported into SQLite.
- Game lookup works by appid and name using SQLite/FTS.
- User's Steam library can be scanned read-only.
- At least one installed Steam game can be detected from manifests.
- Local hardware/OS profile can be detected read-only.
- Reports are ranked with visible recency and system-similarity explanation.
- Launch options/workarounds are extracted and cited.
- Without AI, user can select suggestions and build a launch-option preview.
- AI boundaries exist but do not make core functionality provider-dependent.
- Copy/export works without modifying Steam config.
- Core logic is testable without Wails or frontend dependencies.
