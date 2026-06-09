# AGENTS.md

Project instructions for AI agents working on ProtonSage.

## Product direction

ProtonSage is a local Linux desktop utility for Steam/Proton troubleshooting. ProtonDB/protondb-data is the primary data source. The app should emphasize recent ProtonDB reports strongly: older reports are historical signal, not authoritative truth.

Initial PoC scope is intentionally narrow:

- Import local ProtonDB/protondb-data into SQLite with FTS search.
- Scan the local Steam library for installed games.
- Look up games and rank reports by freshness/relevance.
- Extract launch options mentioned in reports.
- Produce rule-based recommendations/summaries with clear source references.
- Optional but recommended AI advisor can read relevant ProtonDB posts plus local hardware/OS context and propose recommendations with citations.
- Detect local hardware/OS natively in the app so reports can be compared against the user's actual setup.
- Copy/export suggestions only; do not auto-edit Steam config in the first phase.
- The PoC must read the user's Steam library read-only so recommendations can be shown for installed games.

## Planning files

- `PRODUCT_PLAN.md` is the durable product/architecture plan and should keep broad ideas, milestones, and future direction.
- `TODO.md` is the durable implementation backlog.
- `plan.md` is a temporary agent handoff for the current/next implementation slice. Keep it concise and replace/update it as work advances; do not use it as the long-term product plan.

## Working principles

- Simplicity first: prefer small, understandable modules over frameworks or abstractions that are not needed yet.
- Surgical changes: keep edits focused on the requested behavior; avoid broad rewrites.
- Verify success criteria: add or run the smallest useful checks before reporting done.
- No secrets: never read, print, commit, or request secrets/API keys unless explicitly required and approved.
- No speculative features: do not add modes, integrations, daemons, telemetry, cloud sync, or config mutation that were not requested.
- AI integration must be optional. The app must remain useful without AI via deterministic extraction, scoring, and checkbox-based launch-option building.
- If cloud AI is ever supported, require explicit opt-in and show exactly what data is sent. Prefer local/offline AI where practical.
- ProtonForge can be used as an inspiration/reference project, but do not blindly copy architecture or code; preserve license attribution if any code is reused.

## Architecture rules

- Current stack direction: Go backend/core with a Wails desktop shell and a modern web frontend.
- Keep core/domain logic independent of Wails and frontend framework code.
- Frontend code may call Wails-exposed backend methods, but parsing, scoring, storage, Steam scanning, system detection, and advisor logic must live in Go packages that are testable without the GUI.
- The frontend must not directly manipulate local files; all filesystem/Steam operations go through explicit Go backend services.
- Prefer testable functions for parsing, scoring, search, Steam scanning, and recommendation generation.
- Storage and import code should be separable from presentation code.

Suggested dependency direction:

```text
frontend/ -> Wails bindings -> internal/app -> internal/{advisor,core,storage,steam,system,protondb}
internal/advisor -> internal/{core,storage,steam,system}
internal/protondb -> internal/{core,storage}
internal/steam -> internal/core
internal/system -> internal/core
internal/core -> no Wails/frontend dependencies
cmd/protonsage -> internal packages for CLI/dev flows
```

## Data/source rules

- ProtonDB/protondb-data is the primary source for compatibility reports: https://github.com/bdefore/protondb-data
- The raw export repository is large and stores dated `reports_*.tar.gz` snapshots under `reports/`; do not force-clone it unless explicitly needed.
- Default import policy: resolve and download only the latest dated snapshot archive, not every historical archive.
- Current evidence suggests each modern archive is cumulative for reports since the 2019 questionnaire/schema change, not incremental; verify this assumption in importer tests/metadata before relying on it.
- ProtonDB report data is published under ODbL/DbCL; preserve attribution/license notes in docs and exports.
- Any additional data source must be labeled secondary and kept distinguishable in storage and UI.
- Report recency is central to scoring. Recent reports should have much higher weight than stale reports; stale reports can still provide historical context.
- Recommendations must cite the report/source data they are based on.

## Frontend/Wails rules

- The UI should look modern and polished, not like a legacy desktop form.
- Preferred frontend direction: React + Tailwind + shadcn/ui-style components inside Wails.
- Use cards, badges, clear confidence/source states, checkbox/toggle builders, and launch-option preview panels for the recommendation flow.
- Keep Wails bindings thin; put behavior in Go services under `internal/`.

## AI/advisor rules

- AI is optional but recommended; never make core functionality depend on an AI provider.
- AI recommendations must be grounded in retrieved ProtonDB reports, detected hardware/OS context, similarity/difference to report authors' systems, and current app state; do not invent unsupported tweaks.
- AI output must include citations/report references and distinguish confidence levels.
- Without AI, the app should expose extracted tips/snippets as selectable checkboxes so the user can build launch options manually.
- Do not send hardware, OS, Steam library, or report excerpts to an external provider without explicit user opt-in.

## Hardware/OS detection rules

- Detect local system info natively where possible: GPU model/vendor, GPU driver, CPU, RAM, distro, kernel, desktop/session type, and relevant runtime/Steam/Proton context.
- Normalize detected local hardware/OS and ProtonDB `systemInfo` fields so reports can be scored by similarity to the user's system.
- Treat exact hardware matches as useful signal, not absolute truth; recency still matters.
- Keep detection read-only and resilient when commands/files are unavailable.

## Steam/library rules

- Read Steam libraries from normal Linux Steam locations and Flatpak Steam locations where practical.
- Parse `libraryfolders.vdf`, `appmanifest_*.acf`, and existing launch options read-only for the PoC.
- Keep Steam parsing/writing code outside the Wails/frontend layer and test it with fixtures.

## Safety rules

- The first PoC must not write directly to Steam config.
- Any future Steam config write requires explicit user confirmation and a backup made before the change.
- Future direct-write UX should preview exactly what will change and offer restore from backup.
- Prefer copy/export workflows for launch options and recommendations.
