# Phase 03 parent fix report

Phase 03 reviewer returned FAIL for one blocker: quoted environment-variable launch options such as `WINEDLLOVERRIDES="dxgi=n,b" %command%` lost the closing quote during extraction/preview token cleanup.

## Fix applied

- Updated advisor snippet cleanup/token trimming to preserve balanced quotes inside env-var values while still removing unmatched outer quotes/punctuation.
- Added tests for quoted `WINEDLLOVERRIDES="dxgi=n,b"` extraction, citations, and preview composition.

## Validation run

```bash
gofmt -w internal/advisor/extraction.go internal/advisor/advisor_test.go
go test ./...
go test -tags 'wails,webkit2_41' ./...
go run ./cmd/protonsage import-fixture --db "$DB" --fixture testdata/protondb/reports_sample.tar.gz
go run ./cmd/protonsage recommend --db "$DB" --appid 123
go run ./cmd/protonsage launch-preview --db "$DB" --appid 123 --select "$SUGGESTION_ID" --existing "-novid"
git diff --check
git diff --cached --check
```

## Result

PASS. Phase 03 is accepted after the manual blocker fix.
