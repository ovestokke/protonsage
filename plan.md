# Current plan

## Done

- Native Qt6/C++ rewrite is the active stack.
- Old Go/Wails implementation and phase artifacts have been removed.
- Build uses CMake and GitHub Actions.
- ProtonDB snapshot import replaces prior snapshot data atomically.
- Steam scan is read-only and supports native/Flatpak/Snap/env override roots.
- UI shows ProtonDB link, verdict badge, recommended runtime category, suggestions, and copy-only launch preview.

## Next

1. Add GUI flow for downloading/importing the latest ProtonDB snapshot.
2. Add import progress/cancel UX.
3. Continue UI polish without gradients/glow/glass/AI-cockpit styling.
4. Later: optional AI summarization for free-text report notes.
