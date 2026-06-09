# Phase 02 — Steam read-only scan og lokal systemprofil

Bruk denne prompten etter at fase 01 storage/import er på plass, eller når Steam/system-grunnlaget skal fullføres.

## Oppdrag

Ferdigstill ProtonSage sin read-only Steam scan og lokal HW/OS detection slik at appen kan vise installerte spill, eksisterende launch options som kontekst, og sammenligne brukerens system med ProtonDB `systemInfo`.

All Steam-tilgang i PoC er read-only.

## Les først

- `AGENTS.md`
- `PRODUCT_PLAN.md`, særlig “Steam/library plan” og “Hardware/OS profile plan”
- `TODO.md`, særlig Phase 3
- `plan.md`
- `internal/steam/`
- `internal/system/`
- `internal/core/`
- `internal/app/`
- `cmd/protonsage/`
- `testdata/steam/`

## Absolutte rammer

- Ikke skriv til `localconfig.vdf`, `libraryfolders.vdf`, appmanifest eller andre Steam-filer.
- Ikke implementer direkte Steam config mutation i denne fasen.
- Ikke bruk brukerens faktiske Steam-bibliotek i tester.
- Tester skal bruke fixtures/temp dirs.
- Filer skal leses defensivt: manglende Steam installasjon er ikke fatal for appen.
- Core/domain skal ikke importere Wails/frontend.

## Nåværende utgangspunkt

Forvent at disse finnes eller kan finnes:

- Steam root candidate detection for native og Flatpak.
- VDF parser for `libraryfolders.vdf` og `appmanifest_*.acf`.
- `scan-steam --dry-run` CLI.
- System profile detection for GPU/CPU/RAM/distro/kernel/session.

Utvid eksisterende kode. Ikke rewrite alt.

## Arbeidssteg Steam

### 1. Root/library discovery

Sikre at Steam roots håndterer:

- `$HOME/.steam/steam`
- `$HOME/.steam/root`
- `$HOME/.local/share/Steam`
- `$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam`

For hver root:

- les `steamapps/libraryfolders.vdf`
- inkluder root-library selv
- inkluder eksterne library paths
- dedupliser symlinks/duplikater der praktisk
- sorter deterministisk for tester

### 2. App manifests

Parse `steamapps/appmanifest_*.acf` read-only og returner `core.Game`:

- appid
- name
- install dir/path
- library path
- size on disk
- state flags
- build id
- launcher = Steam

Robusthet:

- manglende optional fields er ok
- ugyldig appid gir feil for den manifesten
- én dårlig manifest bør ikke nødvendigvis stoppe hele scan; vurder tydelig error aggregation hvis relevant

### 3. Existing launch options read-only

Implementer valgfritt, men nyttig:

- Les `userdata/<steamid>/config/localconfig.vdf` read-only.
- Finn eksisterende launch options per appid hvis mulig.
- Returner dette som kontekst, ikke som noe som skal skrives tilbake.

Hvis dette blir komplekst, bygg parser/test for et lite fixture-eksempel først.

### 4. Match installed games to DB

Når storage finnes:

- match appid først
- name fallback bare hvis appid mangler eller ikke finnes
- returner status: installed + has ProtonDB reports / missing data

Hold matching-logikk utenfor frontend.

## Arbeidssteg system profile

### 1. Detection

System profile skal være read-only og graceful:

- distro/version fra `/etc/os-release`
- kernel fra `uname -r` eller safe fallback
- CPU fra `/proc/cpuinfo`
- RAM fra `/proc/meminfo`
- GPU/vendor/model fra `lspci` hvis tilgjengelig
- NVIDIA driver fra `nvidia-smi` hvis tilgjengelig
- session/desktop fra env vars
- relevante Steam/Proton context hvis lett tilgjengelig og read-only

Ingen kommando skal være required. Mangler `lspci`, `nvidia-smi`, `glxinfo` osv., returner tomme felter.

### 2. Normalisering

Legg til normalisert profil som kan sammenlignes med ProtonDB `systemInfo`:

- GPU vendor: `nvidia`, `amd`, `intel`, `unknown`
- GPU model simplified hvis mulig
- driver major/minor hvis mulig
- CPU vendor/class hvis mulig
- RAM bucket, f.eks. `<8`, `8-15`, `16-31`, `32+`
- distro family, f.eks. `arch`, `fedora`, `ubuntu`, `debian`, `unknown`
- kernel major/minor
- session type: `wayland`, `x11`, `unknown`

Ikke gjør dette perfekt. Målet er forklarbar similarity, ikke hard sannhet.

### 3. SystemInfo fra ProtonDB

Legg til parser/normalizer for ProtonDB report `systemInfo` map/raw JSON:

- tåle ulike feltvarianter
- returnere samme normaliserte kategorier som lokal profil
- bevare raw fields for citations/debug

## CLI/app-service

Utvid CLI ved behov:

```bash
go run ./cmd/protonsage system-profile
go run ./cmd/protonsage scan-steam --dry-run
go run ./cmd/protonsage scan-steam --dry-run --root testdata/steam/root
```

Hvis storage/matching finnes:

```bash
go run ./cmd/protonsage installed --db /tmp/protonsage.db
```

Expose via `internal/app`:

- `GetSystemProfile()`
- `ScanSteam(...)`
- eventuelt `GetInstalledGames(...)`

Wails-bindinger skal bare delegere.

## Fixtures/tester

Legg til eller utvid fixtures:

```text
testdata/steam/
  native-root/steamapps/libraryfolders.vdf
  native-root/steamapps/appmanifest_123.acf
  secondary-library/steamapps/appmanifest_456.acf
  localconfig.vdf              # hvis launch options leses
```

Test:

- VDF parser håndterer nested objects, quoted strings, comments.
- libraryfolders parser finner root + secondary library.
- appmanifest parser mapper felter riktig.
- scan bruker temp dirs og er read-only.
- localconfig launch options parser, hvis implementert.
- system parsere testes med tekstfixtures/mocked command output, ikke faktisk maskin.
- normalisering av lokal profil og ProtonDB `systemInfo`.

## Tester/verifikasjon

Kjør:

```bash
go fmt ./...
go test ./...
go run ./cmd/protonsage system-profile
go run ./cmd/protonsage scan-steam --dry-run
```

Hvis Wails/frontend påvirkes:

```bash
cd frontend && npm run build
cd .. && go test -tags 'wails,webkit2_41' ./...
```

## Akseptansekriterier

- Steam scan er read-only og testet med fixtures.
- Minst én fixture manifest blir `core.Game` med korrekt appid/name/path metadata.
- Manglende Steam installasjon gir klar melding, ikke crash.
- System profile detection fungerer uten privileges.
- Normalisert system profile finnes for lokal profil og ProtonDB reports.
- Ingen Steam config write path finnes.
- `go test ./...` passerer.

## Sluttrapport

Svar med:

- Steam paths/formats støttet
- system fields/normalisering støttet
- CLI-eksempler
- testresultater
- kjente begrensninger
