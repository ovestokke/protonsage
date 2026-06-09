# Phase 06 — Sluttintegrasjon, røykprøver og fungerende PoC

Bruk denne prompten etter fase 04, eller etter fase 05 hvis AI-boundary også er implementert. Målet er å gjøre ProtonSage til en fungerende PoC fra ende til ende.

## Oppdrag

Gjør en helhetlig gjennomgang av repoet, tett hullene som hindrer PoC-en i å fungere, og legg igjen bevis for at sentrale PoC-kriterier i `PRODUCT_PLAN.md` fungerer:

- latest ProtonDB snapshot metadata
- SQLite import/search
- Steam read-only scan
- system profile detection
- deterministic recommendations
- Wails UI preview/copy flow
- ingen Steam config writes

Ikke legg til nye spekulative features. Fokuser på integrasjon, stabilitet, dokumentasjon og smoke tests.

## Les først

- `AGENTS.md`
- `PRODUCT_PLAN.md`, særlig “PoC success criteria”
- `TODO.md`
- `plan.md`
- `README.md`
- alle relevante pakker under `internal/`
- CLI i `cmd/protonsage/`
- Wails/frontend i `app.go`, `main.go`, `frontend/`

## Absolutte rammer

- Fortsatt copy/export only.
- Ikke implementer Steam config-write.
- Ikke klon protondb-data.
- Ikke last ned alle ProtonDB snapshots.
- Ikke kall ekstern AI som default.
- Ikke les secrets.
- Ikke rydd bort prosjektmapper eller ikke-genererte filer uten eksplisitt approval.

## Arbeidssteg

### 1. Status audit

Lag en kort intern sjekkliste fra TODO/plan:

- Hva er allerede implementert?
- Hva er nødvendig for PoC?
- Hva mangler men er ikke blocker?
- Hvilke tests finnes?
- Hvilke CLI commands finnes?

Ikke bruk mye tid på ny planlegging; målet er å få PoC grønn.

### 2. End-to-end fixture flow

Sikre at en utvikler kan kjøre en lokal fixture-flow:

```bash
go run ./cmd/protonsage import-fixture --db /tmp/protonsage.db --fixture testdata/protondb/<fixture>
go run ./cmd/protonsage lookup --db /tmp/protonsage.db --appid 123
go run ./cmd/protonsage recommend --db /tmp/protonsage.db --appid 123
```

Hvis kommandoene heter noe annet, dokumenter faktiske kommandoer i README.

### 3. Steam/system flow

Sikre at dette fungerer graceful:

```bash
go run ./cmd/protonsage system-profile
go run ./cmd/protonsage scan-steam --dry-run
```

Hvis Steam ikke finnes, skal meldingen være tydelig. Hvis Steam finnes, skal det listes spill read-only.

### 4. Wails/UI smoke

Sikre at frontend og Wails bygger:

```bash
cd frontend && npm install && npm run build
cd ..
go test -tags 'wails,webkit2_41' ./...
wails build -clean
```

Hvis `webkit2_41` ikke passer på maskinen, dokumenter riktig tag/dependency.

UI skal minimum vise:

- installed games/status
- system profile/status
- ProtonDB data status
- selected game recommendation/status
- suggestion checkboxes/toggles
- launch option preview
- copy/export action
- clear safety message: no Steam writes

### 5. Documentation

Oppdater README med:

- exact setup commands
- exact test commands
- exact fixture import/recommend commands
- Wails build notes
- ProtonDB attribution/license note
- safety note: read-only/copy-export
- known limitations

Oppdater TODO:

- marker fullførte punkter
- flytt resterende arbeid til riktig senere fase
- ikke marker noe som ikke faktisk er verifisert

Oppdater `plan.md` til neste realistiske handoff etter smoke-fasen.

### 6. Clean generated artefacts

Rydd bare kjente genererte artefakter:

- `build/`
- `frontend/node_modules/`
- `frontend/dist/assets/`
- `frontend/src/wailsjs/` hvis ignorert/generert
- temp DB-filer

Ikke slett prosjektmapper som `prompts/`, `testdata/`, `internal/`, `frontend/src/`, osv.

## Smoke test script

Hvis nyttig, legg til en enkel script eller dokumentert kommando-sekvens, f.eks. `scripts/smoke.sh`, som kjører:

```bash
go test ./...
cd frontend && npm run build
cd ..
go run ./cmd/protonsage --help
go run ./cmd/protonsage latest-snapshot
go run ./cmd/protonsage system-profile
```

Ikke gjør smoke avhengig av brukerens faktiske Steam library eller nettverk med mindre det er markert optional.

## Akseptansekriterier

- `go test ./...` passerer.
- Frontend build passerer.
- Wails build passerer eller system-dependency blocker er presist dokumentert.
- Fixture import/recommend flow fungerer hvis tidligere faser er implementert.
- Steam scan er read-only og CLI fungerer graceful.
- System profile CLI fungerer uten privileges.
- UI har copy/export-only workflow, ikke apply/write.
- README/TODO/plan er oppdatert til faktisk status.
- Ingen prosjektmapper slettes under cleanup.

## Sluttrapport

Svar med:

- PoC smoke matrix: command → result
- hva som ble fikset/integrert
- hva som fortsatt er begrensning
- nøyaktig neste anbefalte arbeid
