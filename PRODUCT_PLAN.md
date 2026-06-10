# ProtonSage Product Plan

## Goal

Build a native Linux desktop utility that helps users troubleshoot Steam/Proton games using local ProtonDB snapshot data and read-only Steam library detection.

## Non-goals

- No webview/Electron/Wails.
- No Steam config writes in the current app.
- No required AI provider.
- No full clone of `bdefore/protondb-data`.

## Architecture

Native Qt6/C++ application:

```text
src/core/          Domain models
src/storage/       SQLite + FTS schema/query layer
src/protondb/      Snapshot listing/import
src/steam/         Steam root/library/launch-option scanning
src/system/        Local Linux profile detection
src/advisor/       Ranking/extraction/recommendation/preview
src/ui/            Qt widgets UI
```

## Data model and update policy

- ProtonDB data source: `https://github.com/bdefore/protondb-data`
- Import latest `reports/reports_*.tar.gz` snapshot only.
- Snapshot import is a full replacement for prior ProtonDB snapshot data.
- Store raw JSON for future extraction improvements.
- Preserve ODbL/DbCL attribution.

## Recommendation policy

- Recency is primary.
- System similarity is useful but must not override freshness.
- Deterministic extraction should surface launch options, wrappers, env vars, recommended runtime category, and report citations.
- AI summarization may be added later for free-text issues, but deterministic mode must remain useful.

## Steam safety

Current app is read-only:

- Reads library folders, app manifests, and existing launch options.
- Shows/copies launch-option previews.
- Does not write to Steam config.

Future write support requires:

1. explicit opt-in
2. exact diff/preview
3. timestamped backup
4. restore flow

## UI direction

- Native Qt6 widgets.
- Dark flat design: `#1a1a1a` base, `#76B900` accent.
- No gradients/glass/glow/AI-cockpit styling.
- Clear game header with ProtonDB link, computed verdict badge, recommended runtime, suggestions, and launch preview.

## Near-term backlog

- Improve import UI/progress for large snapshots.
- Add explicit snapshot download/import flow in GUI.
- Show rating/runtime badges in sidebar if it remains readable.
- Add more deterministic extraction for save-game/runtime/free-text patterns.
- Add optional local-AI summarization for important report notes.
