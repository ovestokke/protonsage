# ProtonSage — Qt6/C++ Native Rewrite

## Oppdrag

Skriv ProtonSage om fra Go+Wails til ren Qt6/C++. Ingen Go, ingen Wails, ingen webview. Native Linux desktop app med samme funksjonalitet.

## Arkitektur

En CMake-bygg. Alle pakker under `src/`:

```
src/
  core/          — modeller (Game, Report, Suggestion, Recommendation...)
  storage/       — SQLite via Qt SQL
  protondb/      — import fra reports_*.tar.gz snapshot
  steam/         — library scan, VDF-parsing, paths
  system/        — GPU/CPU/RAM/distro detection
  advisor/       — scoring, extraction, recommendations, preview
  ui/            — Qt6 widgets
  main.cpp
schema.sql
CMakeLists.txt
```

## Stil-referanse

Nøyaktig samme stil som ProtonForge (`https://github.com/theinvisible/proton-forge`):

- **Base**: `#1a1a1a`
- **Cards**: `#262626`
- **Input bg**: `#1e1e1e`
- **Border**: `#4a4a4a`
- **Accent**: `#76B900` (NVIDIA-grønn)
- **Text**: `#e0e0e0` (primary), `#d0d0d0` (secondary), `#999` (muted)
- **Ingen** gradients, blur, glassmorphism, glow, skygger — **flatt og solid**.
- QSS-stylesheet (en fil, `style.qss`), ingen inline-styling.

## Krav til UI

- Venstre: game-liste. Hver rad = card med Steam-bilde + navn + appid + report-antall badge.
- Høyre: recommendation. Oppsummering, suggestions med checkbox, env vars/wrappers listet, launch preview nederst.
- Nederst: launch preview bar (copy-knapp, safety-badge).
- Topp: header med ProtonSage-navn + data-status.
- System-profil i venstre sidebar eller egen fane.
- Ingen animasjoner, ingen backdrop-filter, ingen mask-image.
- Scroll må være ekstremt responsivt.

## Funksjonalitet som må porteres fra Go

Følgende fins som fungerende Go-kode. Porter logikken direkte — samme algoritmer, samme edge-cases:

### 1. ProtonDB import
- `internal/protondb/import.go` — håndter nested schema: `app.steam.appId` (string!), `app.title`, `responses.verdict`, `responses.notes` (kan være objekt eller streng), `responses.launchOptions`, `responses.protonVersion`, Unix timestamp
- Import fra `.tar.gz` → `reports_piiremoved.json` stream
- Regresjonstest: `TestImportSnapshotRealProtonDBShape`
- Kilde: `https://github.com/bdefore/protondb-data`

### 2. Steam scan
- `internal/steam/paths.go` — finn Steam roots (~/.steam, Flatpak, osv)
- `internal/steam/library.go` — parse `libraryfolders.vdf`, `appmanifest_*.acf`
- `internal/steam/vdf.go` — VDF-parser
- `internal/system/profile.go` — GPU/CPU/RAM/distro/kernel/session detection
- Installed-game match: appid først, så navn-søk i SQLite FTS

### 3. Storage
- `internal/storage/schema.sql` — SQLite schema med FTS5, import_runs, reports, games, report_system_info
- `internal/storage/queries.go` — alle queries (upsert, insert, search, status, reportsByAppId, etc)
- Bruk Qt SQL eller raw SQLite C API

### 4. Advisor
- `internal/advisor/scoring.go` — recency (72%), system similarity (20%), quality (8%)
- `internal/advisor/extraction.go` — 25 workaround regex-mønstre, env-var extraction, wrapper detection, quoted env-var preservation, conflict detection
- `internal/advisor/recommendation.go` — GenerateRecommendation()
- `internal/advisor/preview.go` — BuildLaunchPreview() med deduping

### 5. Core models
- `internal/core/models.go` — Game, Report, Suggestion, Recommendation, SystemProfile, PreviewResult, Citation, RankedReport...
- Alle med `operator<<` for JSON-serialisering

## Test-krav

- ProtonDB import: les ekte `report_piiremoved.json` fra test-fixture
- Steam VDF: parse `testdata/steam/libraryfolders.vdf`
- Scoring: recency med fast `now`, freshness-labels, system similarity
- Extraction: quoted env vars, conflict detection, env var splitting
- Preview: deduping, ordering, existing launch options

## Ikke gjør

- Ikke skriv Steam config
- Ikke bruk ekstern AI/API
- Ikke legg inn gradients, blur, glassmorphism, animasjoner
- Ikke bruk webview/electron/cef
- Ikke porter Wails-boilerplate

## Akseptansekriterier

- `cmake .. && make -j$(nproc)` bygger uten feil
- `ctest` — alle tester grønne
- UI-en starter og viser spill-liste fra Steam
- Recommendations vises med suggestions, checkboxes, launch preview
- Launch preview kan kopieres
- Scroll yter bra — ingen WebKitGTK-jank
- Design matcher ProtonForge-stil (flatt, mørkt, #76B900 aksent)
