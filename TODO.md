# ProtonSage TODO

## Build/CI

- [x] Native Qt6/C++ build with CMake.
- [x] GitHub Actions Linux build pipeline.
- [x] Import dedup regression test.
- [x] CI-safe smoke test with empty database.

## Data/import

- [x] Import ProtonDB snapshot tar.gz without cloning full repo.
- [x] Store raw report JSON.
- [x] Replace prior ProtonDB snapshot data atomically on import.
- [ ] Add GUI flow for checking/downloading/importing latest snapshot.
- [ ] Add visible import progress and cancellation.

## Steam/library

- [x] Discover native, Flatpak, Snap, and env-override Steam roots.
- [x] Parse `libraryfolders.vdf` and `appmanifest_*.acf`.
- [x] Read existing launch options from `localconfig.vdf`.
- [ ] Consider optional UI picker for unusual Steam roots.

## Recommendations

- [x] Rank reports by recency/system similarity/quality.
- [x] Extract launch options, env vars, wrappers, and known workarounds.
- [x] Show current Steam launch options as pre-checked suggestions.
- [x] Compute verdict badge from recent reports with fallback windows.
- [x] Show recommended runtime category (`Native`, `Official Proton`, `Proton-GE`, `Proton Experimental`).
- [ ] Improve deterministic handling of free-text issues such as save migration notes.

## UI

- [x] Native Qt6 UI, no webview.
- [x] Steam header image cache.
- [x] ProtonDB link under game title.
- [x] Copy-only launch preview.
- [ ] Sidebar rating/runtime badges if visually clean.
- [ ] Continue layout polish.

## Future AI

- [ ] Optional local/offline AI summarization for report notes.
- [ ] Explicit opt-in and payload preview for any external AI provider.
