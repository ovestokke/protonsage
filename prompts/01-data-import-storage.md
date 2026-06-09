# Phase 01 — ProtonDB dataimport, SQLite og søk

Bruk denne prompten etter fase 00 i `/home/ove/projects/protonsage`.

## Oppdrag

Implementer den første ekte data-/storage-slicen. Etter fasen skal ProtonSage kunne initialisere SQLite, importere en liten ProtonDB-fixture, og hente rapporter/spill igjen via testbare Go-pakker og enkle CLI-kommandoer.

Full import av stor ProtonDB-archive kan komme etter at fixture-importen er grønn. Standard oppførsel skal aldri laste ned hele historikken eller klone repoet.

## Les først

- `AGENTS.md`
- `PRODUCT_PLAN.md`, særlig “ProtonDB data plan” og “PoC success criteria”
- `TODO.md`, særlig Phase 1
- `plan.md`, som beskriver nåværende neste slice
- `internal/protondb/`
- `internal/storage/`
- `internal/core/`
- `internal/app/`
- `cmd/protonsage/`
- `README.md`

## Absolutte rammer

- Ikke klon `bdefore/protondb-data`.
- Ikke last ned alle snapshots.
- Ikke last ned stor ProtonDB-archive i tester.
- Tester skal bruke lokal tiny fixture.
- ProtonDB/protondb-data er primærkilde og må merkes med ODbL/DbCL-attribution.
- Import metadata skal lagre snapshot filename/date/source URL/import time/license note.
- Behold core/domain uavhengig av Wails/frontend.
- Ikke skriv Steam config.
- Ikke kall AI.

## Nåværende utgangspunkt

Forvent at følgende kan finnes:

- `internal/protondb/snapshots.go` med latest-snapshot resolver.
- `internal/storage/schema.sql` med SQL-utkast.
- CLI `latest-snapshot`.
- Core models i `internal/core`.

Ikke rewrite fungerende kode. Utvid kirurgisk.

## Beslutning: SQLite-driver

Velg SQLite-driver bevisst før implementering:

- Foretrukket for enkel distribusjon: `modernc.org/sqlite` fordi den er pure Go.
- Alternativ: `github.com/mattn/go-sqlite3` hvis CGO er akseptabelt.

Dokumenter valget kort i README eller sluttrapport. Ikke legg inn ORM nå.

## Foreslåtte filer

```text
internal/storage/
  db.go
  queries.go
  db_test.go
internal/protondb/
  import.go
  import_test.go
testdata/protondb/
  reports_piiremoved.json
  # eller generer reports_sample.tar.gz i test fra JSON-fixture
```

Unngå binære fixtures hvis det er enklere å generere `tar.gz` i testen. Hvis du committer en tar.gz fixture, hold den minimal.

## Storage-krav

### Database init

Implementer noe à la:

```go
func Open(path string) (*DB, error)
func OpenInMemory() (*DB, error)
func (db *DB) Close() error
```

- Bruk `database/sql`.
- Apply `schema.sql` idempotent.
- Bruk transaksjoner for import.

### Query-/write-funksjoner

Legg til små funksjoner for:

- upsert source
- create/finish import run
- upsert game
- insert report
- insert/update report system info
- lookup game by appid
- lookup reports by appid sorted newest-first
- basic text/name search hvis tid

Ikke bygg avansert repository/ORM-lag. Hold API-en konkret.

### FTS

Planen krever FTS for game names/report text. Implementer hvis praktisk i denne fasen:

- `games_fts`
- `reports_fts`

Hvis FTS blir for mye, bygg basic `LIKE` search først og legg tydelig TODO/test for FTS neste.

## ProtonDB importer-krav

Importer skal kunne lese modern archive shape:

```text
reports_*.tar.gz
  reports_piiremoved.json
```

Implementer fra `io.Reader`, ikke bare filsti:

```go
func ImportSnapshot(ctx context.Context, db *storage.DB, r io.Reader, meta SnapshotImportMeta) (ImportResult, error)
```

Minimum importerfelter:

- appid
- title/name hvis tilgjengelig
- timestamp
- rating/verdict
- notes/report text
- launch options hvis tilgjengelig
- proton version hvis tilgjengelig
- systemInfo raw og normaliserbare felter
- source/report identifiers

ProtonDB JSON kan variere. Importer må være tolerant:

- ukjente felter ignoreres
- manglende optional felter gir tomme verdier
- ugyldig timestamp gir forklarlig feil eller record skip med teller, ikke panic

## Fixture-krav

Lag en tiny fixture med minst 2 appids og minst 3 rapporter:

- AppID 123: to rapporter med ulike timestamps, launch option snippets og `systemInfo`.
- AppID 456: én rapport.
- Minst én NVIDIA-ish og én AMD-ish systemInfo.
- Minst én rapport med `%command%` eller env var i notes/launchOptions.

Tester skal bevise:

- import metadata lagres
- games opprettes
- reports opprettes
- `systemInfo` lagres
- lookup by appid virker
- reports sorteres newest-first
- tests laster ikke nettverk

## CLI-krav

Hvis storage/import er grønt, legg til:

```bash
go run ./cmd/protonsage import-fixture --db /tmp/protonsage.db --fixture testdata/protondb/reports_sample.tar.gz
```

eller:

```bash
go run ./cmd/protonsage import-snapshot --db /tmp/protonsage.db --file path/to/reports_x.tar.gz
```

Og lookup:

```bash
go run ./cmd/protonsage lookup --db /tmp/protonsage.db --appid 123
```

Kommandoene skal:

- ikke laste ned noe uten eksplisitt flagg
- skrive kun til valgt DB-fil
- ikke berøre Steam-filer

## App facade/Wails

Hvis naturlig, eksponer minimal status via `internal/app`:

```go
GetDataStatus(dbPath string) (...)
```

Ikke bind direkte storage-detaljer til frontend ennå. Wails-bindinger skal fortsatt være tynne.

## Tester/verifikasjon

Kjør:

```bash
go fmt ./...
go test ./...
go run ./cmd/protonsage latest-snapshot
go run ./cmd/protonsage import-fixture --db /tmp/protonsage-fixture.db --fixture <fixture>
go run ./cmd/protonsage lookup --db /tmp/protonsage-fixture.db --appid 123
```

Hvis frontend/Wails påvirkes:

```bash
cd frontend && npm run build
cd .. && go test -tags 'wails,webkit2_41' ./...
```

## Akseptansekriterier

- `go test ./...` passerer.
- Tiny ProtonDB fixture importeres til SQLite.
- Imported reports kan hentes by appid og sorteres newest-first.
- Import metadata med snapshot/source/license finnes.
- Ingen test bruker nettverk eller stor archive.
- Ingen clone av protondb-data.
- Eksisterende CLI-kommandoer fortsetter å virke.
- Ingen Steam config write path legges til.

## Sluttrapport

Svar med:

- SQLite-driver valgt og hvorfor
- filer endret/opprettet
- CLI-eksempler som virker
- testkommandoer og resultater
- eventuelle TODOs, spesielt FTS hvis utsatt
