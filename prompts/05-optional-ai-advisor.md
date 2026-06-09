# Phase 05 — Valgfri AI-advisor med personvern og citations

Bruk denne prompten etter at den deterministiske advisor-flyten fungerer. Denne fasen skal ikke gjøre appen avhengig av AI.

## Oppdrag

Implementer en trygg optional AI advisor boundary rundt eksisterende deterministisk evidens. Standardmodus skal fortsatt være no-AI. AI skal bare kunne brukes når bruker eksplisitt aktiverer det, og output må være strukturert, validert og sitert.

Målet er provider-grense, privacy disclosure, prompt/context assembly og strukturert validering — ikke fancy provider-integrasjon først.

## Les først

- `AGENTS.md`, særlig AI/advisor rules
- `PRODUCT_PLAN.md`, særlig “Optional AI mode”
- `TODO.md`, særlig Phase 5
- `plan.md`
- `internal/advisor/`
- `internal/core/`
- `internal/app/`
- eksisterende deterministic recommendation flow
- `frontend/` hvis UI toggles skal legges til

## Absolutte rammer

- Appen skal fungere fullt uten AI.
- Ikke kall ekstern/cloud AI som standard.
- Ikke les API-nøkler, token-filer, key stores eller secrets uten eksplisitt brukerapproval.
- Ikke send hardware, OS, Steam library eller report excerpts til ekstern provider uten eksplisitt opt-in.
- AI output må cite kilder og ikke finne på unsupported tweaks.
- AI skal bare bruke bounded context fra eksisterende evidens.
- Deterministisk advisor skal forbli fallback/source of truth.

## Arkitektur

Legg provider boundary i `internal/advisor`, ikke i frontend.

Foreslått struktur:

```text
internal/advisor/
  ai.go                     # interfaces/types
  context.go                # bounded context assembly
  prompt.go                 # prompt construction
  validate.go               # structured output validation
  local_stub.go             # no-op/mock provider for tests
  ai_test.go
```

Eksempeltyper:

```go
type AIProvider interface {
    GenerateRecommendation(ctx context.Context, input AIRequest) (AIResponse, error)
}

type AIRequest struct {
    Game core.Game
    SystemProfile core.SystemProfile
    RankedReports []RankedReport
    Suggestions []core.Suggestion
    ExistingLaunchOptions string
    PrivacyDisclosure PrivacyDisclosure
}

type AIResponse struct {
    Recommendation string
    ExactLaunchOptions string
    Rationale string
    Confidence string
    Risks []string
    Citations []core.Citation
}
```

Ikke bind deg til én provider nå. Bygg interface og testbar pipeline.

## Privacy disclosure

Implementer en struktur som kan vises i UI før ekstern AI aktiveres:

- game appid/name
- local hardware/OS fields som sendes
- existing launch options hvis inkludert
- ProtonDB report snippets som sendes
- deterministic suggestions som sendes
- provider name/type
- om data forlater maskinen

Ekstern AI krever eksplisitt opt-in i UI/CLI. Local/offline provider kan ha mildere disclosure, men bør fortsatt vises.

## Bounded context assembly

AI-context skal være begrenset og evidensbasert:

- selected game identity
- detected hardware/OS summary
- report-vs-user similarity explanations
- top N fresh reports
- extracted snippets/suggestions
- stale report warnings
- existing launch options read-only context

Ikke send hele Steam library. Ikke send hele ProtonDB archive. Ikke send store rå blobs hvis små snippets holder.

Legg til limits:

- max reports, f.eks. 8–12
- max chars per report/snippet
- max total context chars
- deterministic ordering for tests

## Prompt construction

Prompten skal be modellen om:

- kort grounded recommendation
- exact launch options hvis relevant
- rationale
- confidence
- risks/conflicts
- citations using provided IDs only
- ikke invente tweaks
- si “insufficient evidence” hvis evidence ikke støtter anbefaling

Prompten skal inneholde kilde-IDer og kreve at output bruker dem.

## Structured output validation

Implementer validering som avviser eller markerer output hvis:

- citations refererer til ukjente report IDs
- exact launch options inneholder snippet som ikke finnes i evidence eller deterministic suggestions
- confidence er utenfor allowed enum
- required fields mangler
- output anbefaler direkte Steam config write

Validation skal være testbar uten live provider.

## CLI/UI

CLI kan få en safe preview-kommando, men ikke live external call som default:

```bash
go run ./cmd/protonsage ai-context --db /tmp/protonsage.db --appid 123
```

Eventuell live provider må ha eksplisitt flagg:

```bash
go run ./cmd/protonsage ai-recommend --db /tmp/protonsage.db --appid 123 --provider local
```

Ikke implementer `--provider openai` eller lignende uten eksplisitt brukerbeslutning og secret-handling design.

Frontend:

- AI toggle default off.
- Show disclosure modal/panel before enabling external provider.
- Clearly label AI suggestions vs rules suggestions.
- Rules suggestions remain visible.

## Tester

Legg til tester for:

- context includes only selected game, not entire library
- max report/snippet limits
- privacy disclosure lists exact fields
- prompt includes citation IDs
- validator rejects unknown citations
- validator rejects invented launch options
- validator accepts valid structured mock output
- no provider call occurs unless explicitly requested

Bruk mock provider/stub.

## Tester/verifikasjon

Kjør:

```bash
go fmt ./...
go test ./...
```

Hvis UI endres:

```bash
cd frontend && npm run build
cd .. && go test -tags 'wails,webkit2_41' ./...
```

## Akseptansekriterier

- AI provider interface finnes og er optional.
- Appen fungerer uten provider.
- Privacy disclosure kan genereres før AI-kall.
- Prompt/context assembly er bounded og testet.
- Structured output validation er testet.
- Ingen ekstern AI-kall skjer i tester/default flows.
- AI output kan ikke aksepteres uten citations til kjent evidence.
- Ingen Steam write path legges til.

## Sluttrapport

Svar med:

- provider boundary design
- privacy disclosure fields
- validation rules
- testresultater
- hva som fortsatt mangler før live provider kan aktiveres
