# Phase 03 — Søk, ranking og deterministisk anbefalingsflyt

Bruk denne prompten etter at storage/import og Steam/system-grunnlaget finnes.

## Oppdrag

Implementer den ikke-AI-baserte advisor-flyten. ProtonSage skal kunne finne relevante ProtonDB-rapporter for et spill, vektlegge ferskhet sterkt, sammenligne rapportenes `systemInfo` mot brukerens lokale system, trekke ut launch options/workarounds og bygge en copy/export-klar preview fra valgte forslag.

Denne fasen skal gi nyttige anbefalinger uten AI.

## Les først

- `AGENTS.md`
- `PRODUCT_PLAN.md`, særlig “Advisor design”, “Hardware/OS profile plan” og “PoC success criteria”
- `TODO.md`, særlig Phase 2
- `plan.md`
- `internal/advisor/`
- `internal/core/`
- `internal/storage/`
- `internal/protondb/`
- `internal/system/`
- `internal/app/`

## Absolutte rammer

- Ikke kall AI-provider.
- Ikke legg inn cloud-kall.
- Ikke skriv Steam config.
- Recommendations må cite kilder/rapporter.
- Recency er sentralt: ferske rapporter veier klart høyere enn gamle.
- Stale reports skal merkes som historisk kontekst, ikke autoritativ sannhet.
- Hardware/system similarity er forklarende sekundærsignal, ikke viktigere enn recency.
- Core/advisor skal ikke importere Wails/frontend.

## Forventet dataflyt

```text
selected game/appid
  -> storage query reports by appid/name
  -> rank reports by recency + system similarity + confidence signals
  -> extract launch option/workaround snippets
  -> group/dedupe/conflict-detect suggestions
  -> Recommendation{summary, suggestions, citations, generatedBy: rules}
  -> launch option preview from selected suggestions
```

## Arbeidssteg

### 1. Report ranking

Implementer ranking i `internal/advisor` eller egen ren pakke.

Minimum scorekomponenter:

- Recency score:
  - veldig ferske rapporter høy score
  - rapporter eldre enn f.eks. 1 år faller kraftig
  - gamle rapporter kan fortsatt bidra med historisk signal/snippets
- System similarity:
  - GPU vendor/model/driver
  - distro family
  - kernel
  - RAM bucket
  - session type hvis relevant
- Report quality/confidence:
  - rating/verdict hvis tilgjengelig
  - reports med konkrete launch options/tweaks får ekstra forklaringsverdi

Ikke overkompliser modellen. Lag tydelige funksjoner med tester:

```go
RecencyScore(reportTime, now time.Time) float64
SystemSimilarity(user, report NormalizedSystemProfile) SimilarityResult
RankReports(reports []core.Report, profile core.SystemProfile, now time.Time) []RankedReport
```

### 2. Stale report handling

Definer stale thresholds, f.eks.:

- fresh: < 90 dager
- recent: < 365 dager
- stale: >= 365 dager
- historical: >= 2 år

Bruk thresholds i summary/UI fields. Gamle rapporter skal ikke bare filtreres bort; de skal merkes tydelig.

### 3. Launch option/workaround extraction

Trekk ut kandidater fra:

- eksplisitt `launchOptions`
- linjer med `%command%`
- miljøvariabler før `%command%`, f.eks. `PROTON_ENABLE_NVAPI=1`, `DXVK_ASYNC=1`
- wrapper commands, f.eks. `gamemoderun`, `mangohud`, `gamescope`
- kjente workaround-fraser i notes, hvis de kan gjøres om til tydelige snippets

Prioritet:

1. eksplisitt launchOptions-felt
2. full `%command%` lines
3. kjente env vars/wrappers
4. tekstlige tips som ikke er trygge launch options, merket som `note`/`workaround`, ikke automatisk preview

Krav:

- behold exact snippet
- cite source report
- group duplicates/case variants forsiktig
- ikke invent snippets
- ikke inkluder farlige/destruktive shellkommandoer
- konflikt-detect enkle åpenbare konflikter, f.eks. flere alternative `PROTON_` flags med ulike verdier

### 4. Suggestions

Konverter ekstraksjoner til `core.Suggestion`:

- snippet
- kind: `launch_option`, `env_var`, `wrapper`, `workaround`, `diagnostic`, `note`
- sources/citations
- occurrences
- recency_score
- system_similarity
- confidence: `high`, `medium`, `low`
- conflict_notes

Confidence bør være regelbasert og forklarbar:

- høy: flere ferske kilder, similar system, konkret launch option
- medium: én fersk kilde eller flere blandede kilder
- lav: gammel kilde, system mismatch, diagnostic-only, eller svakt tekstlig tips

### 5. Recommendation summary

Generer en deterministisk summary:

- én kort anbefaling
- hvorfor: ferskhet + system match/mismatch
- hva som er usikkert
- stale warning hvis relevant
- ingen AI-språk eller “I think”

Output skal kunne vises i UI og CLI som JSON.

### 6. Launch option preview builder

Implementer en funksjon som bygger preview fra valgte suggestions:

```go
BuildLaunchPreview(selected []core.Suggestion, existing string) PreviewResult
```

Krav:

- inkluder `%command%` én gang
- wrappers/env vars før `%command%`
- notes/workarounds som ikke er launch-option snippets skal ikke automatisk inn i preview
- bevar eksisterende launch options som kontekst hvis gitt
- returner warnings/conflicts
- ingen skriving til Steam

## CLI/app-service

Legg til én eller flere kommandoer når storage finnes:

```bash
go run ./cmd/protonsage recommend --db /tmp/protonsage.db --appid 123
go run ./cmd/protonsage launch-preview --db /tmp/protonsage.db --appid 123 --select <ids>
```

Expose via `internal/app`:

- `GetRecommendation(appid, dbPath)` eller tilsvarende
- `BuildLaunchPreview(...)`

Ikke gjør frontend direkte avhengig av storage.

## Tester

Legg til tests for:

- recency score med faste `now` timestamps
- stale report labeling
- system similarity exact/vendor/mismatch
- ranking der en fersk mismatch slår en veldig gammel exact match, men exact match forklares
- extraction av `%command%`
- extraction av env vars/wrappers
- duplicate grouping
- conflict notes
- preview composition med `%command%` kun én gang
- citations beholdes fra suggestion til recommendation

Bruk tiny ProtonDB fixture fra fase 01 eller egne in-memory report structs.

## Tester/verifikasjon

Kjør:

```bash
go fmt ./...
go test ./...
go run ./cmd/protonsage recommend --db /tmp/protonsage-fixture.db --appid 123
```

Hvis Wails/frontend påvirkes:

```bash
cd frontend && npm run build
cd .. && go test -tags 'wails,webkit2_41' ./...
```

## Akseptansekriterier

- En appid med fixture reports gir deterministisk `Recommendation` med suggestions og citations.
- Report ranking viser/returnerer recency og system-similarity forklaring.
- Launch options/workarounds trekkes ut og cites.
- User kan bygge preview fra valgte suggestions uten AI.
- Stale reports markeres som historiske.
- Ingen AI-provider eller Steam write path er lagt til.
- `go test ./...` passerer.

## Sluttrapport

Svar med:

- ranking/scoring-regler implementert
- extraction-regler implementert
- CLI/API som ble lagt til
- testresultater
- kjente edge cases/TODOs
