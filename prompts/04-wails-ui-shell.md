# Phase 04 — Wails UI shell og brukerflyt

Bruk denne prompten etter at core/storage/Steam/system/advisor har nok data til å vise en ekte flyt, eller for å forbedre eksisterende Wails smoke UI.

## Oppdrag

Bygg en minimal, men polert Wails + React + Tailwind + shadcn/ui-stil frontend som bruker Go-backendens tjenester til å vise installerte spill, systemprofil, ProtonDB-status og deterministiske anbefalinger med selectable launch-option preview.

UI-en skal være sexy/moderne og tillitsvekkende, ikke legacy desktop eller generisk 1999-form.

## Les først

- `AGENTS.md`, særlig Frontend/Wails rules
- `PRODUCT_PLAN.md`, særlig produktretning, Advisor design og PoC success criteria
- `TODO.md`, særlig Phase 4
- `plan.md`
- `internal/app/`
- `app.go`
- `main.go`
- `wails.json`
- `frontend/`
- `README.md`

Hvis en frontend-design skill er tilgjengelig i agentmiljøet, bruk den før du designer komponentene.

## Absolutte rammer

- Frontend skal ikke lese/skrive lokale filer direkte.
- All Steam/filesystem/storage/advisor-logikk går via Go backend methods.
- Wails bindings i `app.go` skal være tynne.
- Ikke skriv Steam config.
- Ikke kall ekstern AI.
- Ikke send hardware/Steam/report data til nettverk.
- Behold appen fungerende uten AI.
- Ikke legg sensitive data i logs.

## Designretning

Velg og gjennomfør en tydelig visuell retning:

- mørk, premium, lokal “compatibility cockpit”
- cards, badges, confidence states, source/citation affordances
- tydelige safe/copy/export states
- moderne typografi og spacing
- ikke standard lilla SaaS-gradient
- ikke bare shadcn defaults uten karakter

Bruk React + Tailwind + shadcn/ui-stil komponenter. Det er greit å ha lokale komponenter fremfor å installere full shadcn hvis det holder repoet enklere.

## Hovedskjermer/komponenter

### 1. App shell

- Header/topbar med ProtonSage navn, data status og safety badge.
- Venstre panel eller sidebar for installed games/search.
- Main panel for selected game recommendation.
- Høyre/sekundært panel for system profile og source details hvis nyttig.

### 2. Installed games/search

Data via Go backend:

- read-only Steam scan status
- installed games list
- search/filter by name
- status badges:
  - installed
  - ProtonDB reports available
  - missing imported data

UX:

- loading state
- no Steam found state
- no imported data state
- error state med trygg forklaring

### 3. ProtonDB/data status

Vis:

- latest snapshot filename/date hvis kjent
- import status/count hvis storage finnes
- source attribution: ProtonDB/protondb-data, ODbL/DbCL note
- warning hvis data ikke er importert

### 4. System profile panel

Vis read-only local profile:

- GPU vendor/model/driver
- CPU
- RAM
- distro/kernel
- session/desktop

Bruk badges og fallback states. Ikke gjør hardware til hemmelig telemetry; vis at alt er lokalt.

### 5. Recommendation panel

For valgt spill:

- summary
- freshness explanation
- system similarity notes
- ranked reports eller top evidence
- source citations
- suggestions med checkbox/toggle
- confidence badges
- conflict warnings
- stale/historical warnings

### 6. Launch-option preview

- Live preview basert på valgte suggestions.
- `%command%` tydelig.
- Copy button.
- Export/copy only.
- Ingen “Apply to Steam” i PoC.
- Hvis eksisterende launch options leses read-only, vis “existing” separat fra “proposed”.

## Wails backend methods

Legg til metoder i `internal/app` først, så bind tynt i `app.go`:

Eksempler:

```go
GetAppInfo()
GetSystemProfile()
GetSteamLibraries()
GetInstalledGames()
GetDataStatus()
GetRecommendation(appid int)
BuildLaunchPreview(request PreviewRequest)
```

Ikke eksponer `db *sql.DB` eller lavnivå storage direkte til frontend. Bruk request/response structs med JSON-tags.

## Frontend data access

Lag et lite wrapperlag, f.eks.:

```text
frontend/src/lib/wails.ts
```

- typed wrappers rundt Wails-generated methods
- fallback/mock data bare for browser/Vite preview
- ingen direkte filesystem API

Hvis Wails genererer `frontend/src/wailsjs`, ikke committ genererte artefakter hvis `.gitignore` sier de er build output, med mindre prosjektet bevisst vil versjonere dem.

## UX states som må håndteres

- første launch uten DB/import
- ProtonDB metadata tilgjengelig, men ingen import
- Steam ikke funnet
- Steam funnet, ingen spill
- system profile partial/missing GPU tools
- recommendation missing because no reports
- conflict in launch suggestions
- copy success/failure

## Tester/verifikasjon

Frontend:

```bash
cd frontend
npm install
npm run build
```

Backend:

```bash
go test ./...
go test -tags 'wails,webkit2_41' ./...
```

Wails:

```bash
wails build -clean
```

Hvis lokal maskin krever WebKitGTK 4.1, behold `build:tags` i `wails.json` som `wails,webkit2_41`. Hvis build feiler pga system dependencies, dokumenter presist.

## Akseptansekriterier

- Frontend bygger med `npm run build`.
- Wails-root bygger med relevante tags eller blocker dokumenteres.
- UI bruker Go backend methods, ikke direkte filtilgang.
- Installed games/search UI finnes eller har ekte wired placeholder med backend states.
- Recommendation panel viser suggestions/citations når data finnes.
- Launch preview kan bygges og kopieres uten Steam writes.
- UI har tydelig safety/copy-only messaging.
- Core packages har fortsatt ingen Wails/frontend imports.

## Sluttrapport

Svar med:

- UI flows/screens implementert
- backend methods lagt til
- frontend build/Wails build resultater
- screenshots hvis miljøet støtter det, ellers beskrivelse
- kjente mangler
