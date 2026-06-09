# Phase 00 — Repository bootstrap og utviklerskjelett

Bruk denne prompten som én implementeringsfase i `/home/ove/projects/protonsage`.

## Oppdrag

Gjør repoet til et lite, ekte og testbart ProtonSage-skjelett. Målet er at backend/CLI og Wails-retningen er på plass før dataimport og anbefalinger bygges videre.

Denne fasen kan allerede være helt eller delvis utført. Ikke anta tomt repo. Bevar fungerende kode og fyll hullene kirurgisk.

## Les først

- `AGENTS.md`
- `PRODUCT_PLAN.md`
- `TODO.md`
- `plan.md`
- `README.md` hvis den finnes
- Eksisterende filer under `cmd/`, `internal/`, `frontend/`, `testdata/`

## Absolutte rammer

- Stack-retning er Go backend/core + Wails v2 desktop shell + moderne web frontend.
- Core/domain-logikk skal være uavhengig av Wails/frontend.
- Frontend skal være tynn og ikke lese/skrive lokale filer direkte.
- Ikke skriv Steam config.
- Ikke kall eksterne AI-providere.
- Ikke klon eller last ned hele `bdefore/protondb-data`.
- Ikke legg til spekulative features.

## Forventet struktur

```text
go.mod
README.md
wails.json
main.go                    # Wails entry, tynn
app.go                     # Wails bindings, tynn
cmd/protonsage/
  main.go                  # CLI/dev flows
internal/
  app/                     # service facade brukt av CLI/Wails
  core/                    # modeller og ren domain-logikk
  protondb/                # snapshot-resolver/import senere
  steam/                   # Steam paths/VDF/read-only scan
  system/                  # lokal HW/OS profile detection
  advisor/                 # no-AI advisor + AI boundary senere
  storage/                 # SQLite schema/storage
testdata/
frontend/                  # React/Vite/Tailwind/Wails frontend
```

## Arbeidssteg

### 1. Prosjektgrunnlag

- Opprett eller kontroller `go.mod` med passende modulnavn.
- Opprett `.gitignore` for Go/Wails/frontend-artefakter:
  - `build/`
  - `frontend/node_modules/`
  - `frontend/dist/*` unntatt `.gitkeep`
  - `frontend/src/wailsjs/`
  - lokale `.db`/`.sqlite` filer
- Legg til `README.md` med:
  - produktformål
  - sikkerhetsstatus: read-only Steam, copy/export only
  - dev-kommandoer
  - ProtonDB kilde/lisensnotat
  - Wails/Node/Go requirements

### 2. Core models

Legg til enkle Go-structs i `internal/core`, uten tung logikk:

- `Game`
- `Report`
- `SystemProfile`
- `Citation`
- `Suggestion`
- `Recommendation`

Structs bør ha JSON-tags fordi de skal kunne brukes av CLI og Wails/frontend.

### 3. App facade

Lag `internal/app` som tynn service/fasade:

- `GetAppInfo()`
- `GetSystemProfile(ctx)` når systempakken finnes
- eventuelle enkle read-only scan-metoder senere

`app.go` i Wails-root skal bare delegere til denne fasaden.

### 4. CLI-skjelett

Lag `cmd/protonsage/main.go` med kommandoer:

```text
version
latest-snapshot
system-profile
scan-steam --dry-run [--root ...]
```

Kommandoer som ikke er implementert ennå skal enten utelates eller feile tydelig. Ikke legg inn skrivekommandoer.

### 5. Wails/frontend smoke shell

- Lag `wails.json`.
- Bruk Wails build tag `wails`; hvis lokal Linux trenger WebKitGTK 4.1, bruk `wails,webkit2_41`.
- Lag `main.go` og `app.go` bak `//go:build wails` slik vanlig `go test ./...` ikke må bygge Wails-root.
- Lag en minimal React/Vite/Tailwind frontend.
- UI-en skal se moderne ut, ikke legacy desktop.
- Første skjerm kan være en smoke-test med cards for:
  - safety mode
  - ProtonDB snapshot status
  - Steam scan status
  - system profile status
  - recommendation preview placeholder

### 6. SQLite schema draft

Legg til `internal/storage/schema.sql` med førsteutkast for:

- `sources`
- `import_runs`
- `games`
- `reports`
- `report_system_info`
- `launch_option_suggestions`

Ikke bygg full importer i denne fasen med mindre alt annet er grønt.

## Tester/verifikasjon

Kjør så mye som mulig av:

```bash
go fmt ./...
go test ./...
go run ./cmd/protonsage --help
go run ./cmd/protonsage version
cd frontend && npm install && npm run build
```

Hvis Wails CLI finnes:

```bash
go test -tags 'wails,webkit2_41' ./...
wails build -clean
```

Hvis Wails build feiler pga manglende systempakker, dokumenter nøyaktig dependency-feil og behold backend/CLI grønt.

## Akseptansekriterier

- `go test ./...` passerer.
- CLI help/version virker.
- Core packages har ingen Wails/frontend-avhengighet.
- Wails/frontend-skjelett finnes eller er tydelig dokumentert som neste steg.
- README beskriver sikkerhetsrammene.
- Ingen Steam config write path er implementert.
- Ingen ekstern AI-kall er implementert.

## Sluttrapport

Svar med:

- hva som ble opprettet/endret
- kommandoer som ble kjørt
- hva som passerte/feilet
- eventuelle blockers
- neste anbefalte fase
