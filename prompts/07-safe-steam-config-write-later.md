# Phase 07 — Safe Steam config write workflow (LATER ONLY)

Denne prompten er for en fremtidig fase. Ikke bruk den i PoC uten eksplisitt brukerbeskjed om at direkte Steam config write skal designes og implementeres.

## Oppdrag

Implementer en trygg, eksplisitt og reverserbar workflow for å skrive launch options til Steam config. Dette er ikke en PoC-funksjon. Den skal bare bygges etter at copy/export-flyten fungerer godt og bruker eksplisitt ønsker direkte write.

## Les først

- `AGENTS.md`, særlig Safety rules og Steam/library rules
- `PRODUCT_PLAN.md`, særlig “Safe Steam config write” milestone
- `TODO.md` Later
- eksisterende Steam parser/serializer kode
- eksisterende launch option preview/advisor kode
- ProtonForge reference notes hvis de fortsatt er relevante

## Absolutte rammer

- Krever eksplisitt user confirmation i UI/CLI.
- Krever exact preview/diff før write.
- Krever timestamped backup før write.
- Krever restore workflow.
- Ikke bruk broad regex rewrites på `localconfig.vdf`.
- Bruk real parser/serializer eller en ekstremt smal, testet patcher.
- Ikke skriv hvis Steam kjører, med mindre workflow håndterer dette trygt og eksplisitt.
- Ikke overskriv eksisterende user launch options uten å vise dem og tilby merge/replace-valg.

## Ikke-start før disse kravene er oppfylt

- Deterministisk recommendation + launch preview fungerer.
- Existing launch options kan leses read-only.
- UI kan vise proposed vs existing klart.
- Tests dekker VDF parsing for localconfig.
- Bruker har bedt om direct-write feature.

## Designkrav

### 1. Write plan model

Lag en modell som beskriver endringen uten å utføre den:

```go
type SteamWritePlan struct {
    AppID int
    ConfigPath string
    BackupPath string
    ExistingLaunchOptions string
    ProposedLaunchOptions string
    Mode string // merge, replace, append, clear?
    Diff []DiffLine
    Warnings []string
}
```

Planen skal kunne vises i CLI/UI som JSON før apply.

### 2. Backup

Før enhver write:

- lag backup i en tydelig path, f.eks. `localconfig.vdf.protonsage-YYYYMMDD-HHMMSS.bak`
- fsync/close der praktisk
- verifiser at backup kan leses
- stopp hvis backup feiler

### 3. Apply

Write skal være atomic-ish:

- skriv til temp file i samme directory
- verifiser parse av temp file
- rename over original hvis mulig
- returner backup path og restore instructions

### 4. Restore

Implementer restore fra backup:

- preview hvilken backup som restores
- explicit confirmation
- backup current file før restore hvis tryggest
- tests for restore

### 5. UI/CLI confirmation

CLI bør kreve explicit flags:

```bash
protonsage steam-write-plan --appid 123 --launch-options 'gamemoderun %command%'
protonsage steam-write-apply --plan plan.json --confirm-appid 123 --confirm-backup
protonsage steam-restore --backup path --confirm
```

UI skal kreve tydelig confirmation, f.eks. checkbox + typed appid eller “I understand”.

## Tester

Bruk bare fixtures/temp dirs:

- parse localconfig fixture
- generate write plan without changing file
- backup created before write
- apply updates only target appid
- unrelated config preserved byte-for-byte eller semantisk hvis serializer normaliserer
- malformed config aborts safely
- restore restores backup
- Steam-running guard hvis implementert
- no write without confirmation

## Akseptansekriterier

- Ingen direct write skjer uten explicit confirmation.
- Preview/diff er exact nok til at bruker ser hva som endres.
- Backup lages og path returneres før apply.
- Restore workflow finnes og er testet.
- Fixtures viser at unrelated entries ikke ødelegges.
- `go test ./...` passerer.
- README/Safety docs oppdateres.

## Sluttrapport

Svar med:

- hvordan confirmation fungerer
- backup/restore paths
- parser/serializer-strategi
- testresultater
- kjente risikoer
