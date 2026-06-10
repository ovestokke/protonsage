# AGENTS.md

Project instructions for agents working on ProtonSage.

## Product direction

ProtonSage is a local Linux desktop utility for Steam/Proton troubleshooting.

- Stack: native Qt6/C++.
- No webview, Wails, Electron, CEF, or WebKitGTK dependency.
- ProtonDB/protondb-data is the primary report source: https://github.com/bdefore/protondb-data
- Import only snapshot archives from `reports/`; do not clone the full ProtonDB data repository.
- Scan Steam read-only. Do not write Steam config.
- Core logic must remain independent of UI code where practical.
- App must work without AI through deterministic extraction, scoring, checkboxes, and copy/export.

## Current architecture

```text
src/core/          Shared domain models
src/storage/       SQLite schema and query layer
src/protondb/      Snapshot listing and full-snapshot import
src/steam/         Steam path detection, VDF parser, read-only library scan
src/system/        Read-only Linux hardware/OS detection from /proc and /sys
src/advisor/       Deterministic scoring, extraction, recommendations, preview
src/ui/            Qt6 widgets UI and image cache
```

Dependency direction:

```text
ui -> advisor/storage/steam/system/protondb/core
advisor -> core
storage -> core
protondb -> storage/core
steam -> core
system -> core
core -> no UI dependencies
```

## Build/test

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/import_dedup_test
./build/smoke_test
./build/protonsage
```

For existing build dirs, `cmake --build build` is fine.

CI uses GitHub Actions in `.github/workflows/build.yml`.

## Data rules

- Database path: `$XDG_DATA_HOME/protonsage/protonsage.db`, fallback `~/.local/share/protonsage/protonsage.db`.
- Cache path: `$XDG_CACHE_HOME/protonsage/` or Qt standard cache location.
- ProtonDB snapshots are full replacements. Re-importing a snapshot must not duplicate reports.
- Preserve ProtonDB ODbL/DbCL attribution in docs/import metadata/exports.
- Report recency is central. Recent reports should outweigh stale historical reports.
- Cite/source recommendations from ProtonDB reports.

## Steam rules

- Read-only only.
- Discover roots through `PROTONSAGE_STEAM_ROOTS`, native Steam, Flatpak Steam, and Snap Steam paths.
- Parse `libraryfolders.vdf`, `appmanifest_*.acf`, and `localconfig.vdf` launch options.
- Existing Steam launch options may be displayed and copied; do not mutate them.

## System detection rules

- Use `/proc` and `/sys` where possible.
- Avoid blocking `QProcess` calls in app startup/UI paths.
- Detect GPU vendor/model/driver, CPU, RAM, distro, kernel, desktop/session.

## UI rules

- Design target: dark, flat, `#1a1a1a` base, `#76B900` accent.
- Avoid gradients, blur, glow, glassmorphism, emoji badges, and generic "AI cockpit" aesthetics.
- Use plain labels and clear source-backed evidence.
- Launch options are checkbox-selected and copy/export only.

## AI rules

- AI is optional and future-facing.
- Never make core recommendations dependent on an AI provider.
- Do not send hardware, Steam library, or report excerpts to external providers without explicit opt-in and preview.
