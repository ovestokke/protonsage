package storage

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"time"

	"protonsage/internal/core"
)

const dateLayout = "2006-01-02"

// SourceInput identifies a ProtonDB/protondb-data source snapshot.
type SourceInput struct {
	ID         string
	Kind       string
	URL        string
	License    string
	ImportedAt time.Time
}

// ImportRunInput is persisted before a snapshot import begins.
type ImportRunInput struct {
	SourceID         string
	SnapshotFilename string
	SnapshotDate     time.Time
	SourceURL        string
	License          string
	StartedAt        time.Time
}

// ImportRun records one local import attempt for a ProtonDB snapshot/fixture.
type ImportRun struct {
	ID               int64      `json:"id"`
	SourceID         string     `json:"sourceId"`
	SnapshotFilename string     `json:"snapshotFilename"`
	SnapshotDate     time.Time  `json:"snapshotDate"`
	SourceURL        string     `json:"sourceUrl"`
	License          string     `json:"license"`
	StartedAt        time.Time  `json:"startedAt"`
	FinishedAt       *time.Time `json:"finishedAt,omitempty"`
	ReportCount      int        `json:"reportCount"`
	SkippedCount     int        `json:"skippedCount"`
}

// ReportInput is the concrete storage shape for an imported ProtonDB report.
type ReportInput struct {
	ImportRunID    int64
	SourceReportID string
	RawJSON        string
	Report         core.Report
}

// ReportSystemInfo stores normalized fields plus the original ProtonDB systemInfo JSON.
type ReportSystemInfo struct {
	GPUVendor  string                       `json:"gpuVendor,omitempty"`
	GPUModel   string                       `json:"gpuModel,omitempty"`
	GPUDriver  string                       `json:"gpuDriver,omitempty"`
	CPU        string                       `json:"cpu,omitempty"`
	RAMGB      float64                      `json:"ramGb,omitempty"`
	Distro     string                       `json:"distro,omitempty"`
	Kernel     string                       `json:"kernel,omitempty"`
	Desktop    string                       `json:"desktop,omitempty"`
	RawJSON    string                       `json:"rawJson,omitempty"`
	Normalized core.NormalizedSystemProfile `json:"normalized"`
}

// ReportRecord is a report row joined with normalized system information.
type ReportRecord struct {
	ID             int64            `json:"id"`
	ImportRunID    int64            `json:"importRunId"`
	SourceReportID string           `json:"sourceReportId"`
	RawJSON        string           `json:"rawJson,omitempty"`
	Report         core.Report      `json:"report"`
	SystemInfo     ReportSystemInfo `json:"systemInfo"`
}

// DataStatus is a compact summary of the imported local data set.
type DataStatus struct {
	SourceCount    int        `json:"sourceCount"`
	ImportRunCount int        `json:"importRunCount"`
	GameCount      int        `json:"gameCount"`
	ReportCount    int        `json:"reportCount"`
	LatestImport   *ImportRun `json:"latestImport,omitempty"`
}

func (db *DB) UpsertSource(ctx context.Context, input SourceInput) error {
	return db.queries().UpsertSource(ctx, input)
}

func (q *Queries) UpsertSource(ctx context.Context, input SourceInput) error {
	input.ID = strings.TrimSpace(input.ID)
	input.Kind = strings.TrimSpace(input.Kind)
	input.URL = strings.TrimSpace(input.URL)
	input.License = strings.TrimSpace(input.License)
	if input.ID == "" {
		return errors.New("upsert source: id is required")
	}
	if input.Kind == "" {
		return errors.New("upsert source: kind is required")
	}
	if input.URL == "" {
		return errors.New("upsert source: url is required")
	}
	if input.License == "" {
		return errors.New("upsert source: license is required")
	}
	if input.ImportedAt.IsZero() {
		input.ImportedAt = time.Now().UTC()
	}

	_, err := q.runner.ExecContext(ctx, `
INSERT INTO sources (id, kind, url, license, imported_at)
VALUES (?, ?, ?, ?, ?)
ON CONFLICT(id) DO UPDATE SET
    kind = excluded.kind,
    url = excluded.url,
    license = excluded.license,
    imported_at = excluded.imported_at
`, input.ID, input.Kind, input.URL, input.License, formatTimestamp(input.ImportedAt))
	if err != nil {
		return fmt.Errorf("upsert source %s: %w", input.ID, err)
	}
	return nil
}

func (db *DB) CreateImportRun(ctx context.Context, input ImportRunInput) (int64, error) {
	return db.queries().CreateImportRun(ctx, input)
}

func (q *Queries) CreateImportRun(ctx context.Context, input ImportRunInput) (int64, error) {
	input.SourceID = strings.TrimSpace(input.SourceID)
	input.SnapshotFilename = strings.TrimSpace(input.SnapshotFilename)
	input.SourceURL = strings.TrimSpace(input.SourceURL)
	input.License = strings.TrimSpace(input.License)
	if input.SourceID == "" {
		return 0, errors.New("create import run: source id is required")
	}
	if input.SnapshotFilename == "" {
		return 0, errors.New("create import run: snapshot filename is required")
	}
	if input.SnapshotDate.IsZero() {
		return 0, errors.New("create import run: snapshot date is required")
	}
	if input.SourceURL == "" {
		return 0, errors.New("create import run: source url is required")
	}
	if input.License == "" {
		return 0, errors.New("create import run: license is required")
	}
	if input.StartedAt.IsZero() {
		input.StartedAt = time.Now().UTC()
	}

	res, err := q.runner.ExecContext(ctx, `
INSERT INTO import_runs (source_id, snapshot_filename, snapshot_date, source_url, license, started_at)
VALUES (?, ?, ?, ?, ?, ?)
`, input.SourceID, input.SnapshotFilename, formatDate(input.SnapshotDate), input.SourceURL, input.License, formatTimestamp(input.StartedAt))
	if err != nil {
		return 0, fmt.Errorf("create import run for %s: %w", input.SnapshotFilename, err)
	}
	id, err := res.LastInsertId()
	if err != nil {
		return 0, fmt.Errorf("read import run id: %w", err)
	}
	return id, nil
}

func (db *DB) FinishImportRun(ctx context.Context, id int64, reportCount, skippedCount int) error {
	return db.queries().FinishImportRun(ctx, id, reportCount, skippedCount)
}

func (q *Queries) FinishImportRun(ctx context.Context, id int64, reportCount, skippedCount int) error {
	if id <= 0 {
		return errors.New("finish import run: id is required")
	}
	_, err := q.runner.ExecContext(ctx, `
UPDATE import_runs
SET finished_at = ?, report_count = ?, skipped_count = ?
WHERE id = ?
`, formatTimestamp(time.Now().UTC()), reportCount, skippedCount, id)
	if err != nil {
		return fmt.Errorf("finish import run %d: %w", id, err)
	}
	return nil
}

func (db *DB) UpsertGame(ctx context.Context, game core.Game) error {
	return db.queries().UpsertGame(ctx, game)
}

func (q *Queries) UpsertGame(ctx context.Context, game core.Game) error {
	if game.AppID <= 0 {
		return errors.New("upsert game: appid is required")
	}
	game.Name = strings.TrimSpace(game.Name)
	if game.Name == "" {
		game.Name = "Steam App " + strconv.Itoa(game.AppID)
	}
	if game.Launcher == "" {
		game.Launcher = core.LauncherSteam
	}

	_, err := q.runner.ExecContext(ctx, `
INSERT INTO games (appid, name, launcher)
VALUES (?, ?, ?)
ON CONFLICT(appid) DO UPDATE SET
    name = excluded.name,
    launcher = excluded.launcher
`, game.AppID, game.Name, string(game.Launcher))
	if err != nil {
		return fmt.Errorf("upsert game %d: %w", game.AppID, err)
	}
	return nil
}

func (db *DB) InsertReport(ctx context.Context, input ReportInput) (int64, error) {
	return db.queries().InsertReport(ctx, input)
}

func (q *Queries) InsertReport(ctx context.Context, input ReportInput) (int64, error) {
	if input.ImportRunID <= 0 {
		return 0, errors.New("insert report: import run id is required")
	}
	if input.Report.AppID <= 0 {
		return 0, errors.New("insert report: appid is required")
	}
	if input.Report.SourceID == "" {
		return 0, errors.New("insert report: source id is required")
	}
	if input.Report.Timestamp.IsZero() {
		return 0, errors.New("insert report: timestamp is required")
	}
	input.SourceReportID = strings.TrimSpace(input.SourceReportID)
	if input.SourceReportID == "" {
		input.SourceReportID = fmt.Sprintf("%s:%d:%s", input.Report.SourceID, input.Report.AppID, formatTimestamp(input.Report.Timestamp))
	}
	if strings.TrimSpace(input.RawJSON) == "" {
		input.RawJSON = "{}"
	}

	res, err := q.runner.ExecContext(ctx, `
INSERT INTO reports (
    import_run_id, source_id, source_report_id, appid, title, timestamp,
    verdict, rating, notes, launch_options, proton_version, raw_json
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
`, input.ImportRunID, input.Report.SourceID, input.SourceReportID, input.Report.AppID, input.Report.Title,
		formatTimestamp(input.Report.Timestamp), input.Report.Verdict, input.Report.Rating, input.Report.Notes,
		input.Report.LaunchOptions, input.Report.ProtonVersion, input.RawJSON)
	if err != nil {
		return 0, fmt.Errorf("insert report %s: %w", input.SourceReportID, err)
	}
	id, err := res.LastInsertId()
	if err != nil {
		return 0, fmt.Errorf("read report id: %w", err)
	}
	return id, nil
}

func (db *DB) UpsertReportSystemInfo(ctx context.Context, reportID int64, info ReportSystemInfo) error {
	return db.queries().UpsertReportSystemInfo(ctx, reportID, info)
}

func (q *Queries) UpsertReportSystemInfo(ctx context.Context, reportID int64, info ReportSystemInfo) error {
	if reportID <= 0 {
		return errors.New("upsert report system info: report id is required")
	}
	_, err := q.runner.ExecContext(ctx, `
INSERT INTO report_system_info (
    report_id, gpu_vendor, gpu_model, gpu_driver, cpu, ram_gb, distro, kernel, desktop, raw_json
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(report_id) DO UPDATE SET
    gpu_vendor = excluded.gpu_vendor,
    gpu_model = excluded.gpu_model,
    gpu_driver = excluded.gpu_driver,
    cpu = excluded.cpu,
    ram_gb = excluded.ram_gb,
    distro = excluded.distro,
    kernel = excluded.kernel,
    desktop = excluded.desktop,
    raw_json = excluded.raw_json
`, reportID, info.GPUVendor, info.GPUModel, info.GPUDriver, info.CPU, info.RAMGB, info.Distro, info.Kernel, info.Desktop, info.RawJSON)
	if err != nil {
		return fmt.Errorf("upsert report system info %d: %w", reportID, err)
	}
	return nil
}

func (db *DB) LookupGame(ctx context.Context, appid int) (core.Game, bool, error) {
	return db.queries().LookupGame(ctx, appid)
}

func (q *Queries) LookupGame(ctx context.Context, appid int) (core.Game, bool, error) {
	var game core.Game
	var launcher string
	err := q.runner.QueryRowContext(ctx, `
SELECT appid, name, launcher
FROM games
WHERE appid = ?
`, appid).Scan(&game.AppID, &game.Name, &launcher)
	if errors.Is(err, sql.ErrNoRows) {
		return core.Game{}, false, nil
	}
	if err != nil {
		return core.Game{}, false, fmt.Errorf("lookup game %d: %w", appid, err)
	}
	game.Launcher = core.Launcher(launcher)
	return game, true, nil
}

func (db *DB) ReportsByAppID(ctx context.Context, appid int) ([]ReportRecord, error) {
	return db.queries().ReportsByAppID(ctx, appid)
}

func (db *DB) ReportCountByAppID(ctx context.Context, appid int) (int, error) {
	return db.queries().ReportCountByAppID(ctx, appid)
}

func (q *Queries) ReportCountByAppID(ctx context.Context, appid int) (int, error) {
	if appid <= 0 {
		return 0, nil
	}
	var count int
	if err := q.runner.QueryRowContext(ctx, `SELECT COUNT(*) FROM reports WHERE appid = ?`, appid).Scan(&count); err != nil {
		return 0, fmt.Errorf("count reports for appid %d: %w", appid, err)
	}
	return count, nil
}

func (q *Queries) ReportsByAppID(ctx context.Context, appid int) ([]ReportRecord, error) {
	rows, err := q.runner.QueryContext(ctx, reportSelectSQL+`
WHERE r.appid = ?
ORDER BY r.timestamp DESC, r.id DESC
`, appid)
	if err != nil {
		return nil, fmt.Errorf("lookup reports for appid %d: %w", appid, err)
	}
	defer rows.Close()
	return scanReportRows(rows)
}

func (db *DB) SearchGames(ctx context.Context, query string, limit int) ([]core.Game, error) {
	return db.queries().SearchGames(ctx, query, limit)
}

func (q *Queries) SearchGames(ctx context.Context, query string, limit int) ([]core.Game, error) {
	ftsQuery := buildFTSQuery(query)
	if ftsQuery == "" {
		return nil, nil
	}
	limit = normalizeLimit(limit)

	rows, err := q.runner.QueryContext(ctx, `
SELECT g.appid, g.name, g.launcher
FROM games_fts
JOIN games AS g ON g.appid = games_fts.rowid
WHERE games_fts MATCH ?
ORDER BY bm25(games_fts)
LIMIT ?
`, ftsQuery, limit)
	if err != nil {
		return nil, fmt.Errorf("search games: %w", err)
	}
	defer rows.Close()

	var games []core.Game
	for rows.Next() {
		var game core.Game
		var launcher string
		if err := rows.Scan(&game.AppID, &game.Name, &launcher); err != nil {
			return nil, fmt.Errorf("scan game search result: %w", err)
		}
		game.Launcher = core.Launcher(launcher)
		games = append(games, game)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate game search results: %w", err)
	}
	return games, nil
}

func (db *DB) SearchReports(ctx context.Context, query string, limit int) ([]ReportRecord, error) {
	return db.queries().SearchReports(ctx, query, limit)
}

func (q *Queries) SearchReports(ctx context.Context, query string, limit int) ([]ReportRecord, error) {
	ftsQuery := buildFTSQuery(query)
	if ftsQuery == "" {
		return nil, nil
	}
	limit = normalizeLimit(limit)

	rows, err := q.runner.QueryContext(ctx, reportSelectSQL+`
JOIN reports_fts ON reports_fts.rowid = r.id
WHERE reports_fts MATCH ?
ORDER BY bm25(reports_fts), r.timestamp DESC
LIMIT ?
`, ftsQuery, limit)
	if err != nil {
		return nil, fmt.Errorf("search reports: %w", err)
	}
	defer rows.Close()
	return scanReportRows(rows)
}

func (db *DB) ImportRuns(ctx context.Context) ([]ImportRun, error) {
	return db.queries().ImportRuns(ctx)
}

func (q *Queries) ImportRuns(ctx context.Context) ([]ImportRun, error) {
	rows, err := q.runner.QueryContext(ctx, `
SELECT id, source_id, snapshot_filename, snapshot_date, source_url, license, started_at, finished_at, report_count, skipped_count
FROM import_runs
ORDER BY started_at DESC, id DESC
`)
	if err != nil {
		return nil, fmt.Errorf("list import runs: %w", err)
	}
	defer rows.Close()

	var runs []ImportRun
	for rows.Next() {
		run, err := scanImportRun(rows)
		if err != nil {
			return nil, err
		}
		runs = append(runs, run)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate import runs: %w", err)
	}
	return runs, nil
}

func (db *DB) Status(ctx context.Context) (DataStatus, error) {
	return db.queries().Status(ctx)
}

func (q *Queries) Status(ctx context.Context) (DataStatus, error) {
	status := DataStatus{}
	counts := []struct {
		query string
		dest  *int
	}{
		{`SELECT COUNT(*) FROM sources`, &status.SourceCount},
		{`SELECT COUNT(*) FROM import_runs`, &status.ImportRunCount},
		{`SELECT COUNT(*) FROM games`, &status.GameCount},
		{`SELECT COUNT(*) FROM reports`, &status.ReportCount},
	}
	for _, item := range counts {
		if err := q.runner.QueryRowContext(ctx, item.query).Scan(item.dest); err != nil {
			return DataStatus{}, fmt.Errorf("read data status: %w", err)
		}
	}

	rows, err := q.runner.QueryContext(ctx, `
SELECT id, source_id, snapshot_filename, snapshot_date, source_url, license, started_at, finished_at, report_count, skipped_count
FROM import_runs
ORDER BY started_at DESC, id DESC
LIMIT 1
`)
	if err != nil {
		return DataStatus{}, fmt.Errorf("read latest import run: %w", err)
	}
	defer rows.Close()
	if rows.Next() {
		run, err := scanImportRun(rows)
		if err != nil {
			return DataStatus{}, err
		}
		status.LatestImport = &run
	}
	if err := rows.Err(); err != nil {
		return DataStatus{}, fmt.Errorf("iterate latest import run: %w", err)
	}
	return status, nil
}

const reportSelectSQL = `
SELECT
    r.id, r.import_run_id, r.source_id, r.source_report_id, r.appid, r.title, r.timestamp,
    r.verdict, r.rating, r.notes, r.launch_options, r.proton_version, r.raw_json,
    COALESCE(si.gpu_vendor, ''), COALESCE(si.gpu_model, ''), COALESCE(si.gpu_driver, ''),
    COALESCE(si.cpu, ''), COALESCE(si.ram_gb, 0), COALESCE(si.distro, ''),
    COALESCE(si.kernel, ''), COALESCE(si.desktop, ''), COALESCE(si.raw_json, '')
FROM reports AS r
LEFT JOIN report_system_info AS si ON si.report_id = r.id
`

func scanReportRows(rows *sql.Rows) ([]ReportRecord, error) {
	var records []ReportRecord
	for rows.Next() {
		record, err := scanReport(rows)
		if err != nil {
			return nil, err
		}
		records = append(records, record)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate report rows: %w", err)
	}
	return records, nil
}

type scanner interface {
	Scan(...any) error
}

func scanReport(row scanner) (ReportRecord, error) {
	var record ReportRecord
	var title, verdict, rating, notes, launchOptions, protonVersion sql.NullString
	var timestampText string
	err := row.Scan(
		&record.ID, &record.ImportRunID, &record.Report.SourceID, &record.SourceReportID, &record.Report.AppID,
		&title, &timestampText, &verdict, &rating, &notes, &launchOptions, &protonVersion, &record.RawJSON,
		&record.SystemInfo.GPUVendor, &record.SystemInfo.GPUModel, &record.SystemInfo.GPUDriver,
		&record.SystemInfo.CPU, &record.SystemInfo.RAMGB, &record.SystemInfo.Distro,
		&record.SystemInfo.Kernel, &record.SystemInfo.Desktop, &record.SystemInfo.RawJSON,
	)
	if err != nil {
		return ReportRecord{}, fmt.Errorf("scan report row: %w", err)
	}
	record.Report.ID = record.ID
	record.Report.SourceReportID = record.SourceReportID
	record.Report.Title = title.String
	record.Report.Verdict = verdict.String
	record.Report.Rating = rating.String
	record.Report.Notes = notes.String
	record.Report.LaunchOptions = launchOptions.String
	record.Report.ProtonVersion = protonVersion.String
	record.SystemInfo.Normalized = record.SystemInfo.NormalizedProfile()
	timestamp, err := time.Parse(time.RFC3339, timestampText)
	if err != nil {
		return ReportRecord{}, fmt.Errorf("parse report timestamp %q: %w", timestampText, err)
	}
	record.Report.Timestamp = timestamp
	record.Report.SystemInfo = record.SystemInfo.AsMap()
	return record, nil
}

func scanImportRun(row scanner) (ImportRun, error) {
	var run ImportRun
	var snapshotDate, startedAt string
	var finishedAt sql.NullString
	if err := row.Scan(&run.ID, &run.SourceID, &run.SnapshotFilename, &snapshotDate, &run.SourceURL, &run.License, &startedAt, &finishedAt, &run.ReportCount, &run.SkippedCount); err != nil {
		return ImportRun{}, fmt.Errorf("scan import run: %w", err)
	}
	date, err := time.Parse(dateLayout, snapshotDate)
	if err != nil {
		return ImportRun{}, fmt.Errorf("parse import run snapshot date %q: %w", snapshotDate, err)
	}
	run.SnapshotDate = date
	started, err := time.Parse(time.RFC3339, startedAt)
	if err != nil {
		return ImportRun{}, fmt.Errorf("parse import run started_at %q: %w", startedAt, err)
	}
	run.StartedAt = started
	if finishedAt.Valid && finishedAt.String != "" {
		finished, err := time.Parse(time.RFC3339, finishedAt.String)
		if err != nil {
			return ImportRun{}, fmt.Errorf("parse import run finished_at %q: %w", finishedAt.String, err)
		}
		run.FinishedAt = &finished
	}
	return run, nil
}

func (info ReportSystemInfo) HasValues() bool {
	return strings.TrimSpace(info.GPUVendor) != "" || strings.TrimSpace(info.GPUModel) != "" || strings.TrimSpace(info.GPUDriver) != "" || strings.TrimSpace(info.CPU) != "" || info.RAMGB > 0 || strings.TrimSpace(info.Distro) != "" || strings.TrimSpace(info.Kernel) != "" || strings.TrimSpace(info.Desktop) != ""
}

func (info ReportSystemInfo) NormalizedProfile() core.NormalizedSystemProfile {
	return core.NormalizeSystemInfoValues(core.SystemInfoValues{
		GPUVendor:   info.GPUVendor,
		GPUModel:    info.GPUModel,
		GPUDriver:   info.GPUDriver,
		CPU:         info.CPU,
		RAMGB:       info.RAMGB,
		Distro:      info.Distro,
		Kernel:      info.Kernel,
		SessionType: info.Desktop,
		Desktop:     info.Desktop,
	})
}

func (info ReportSystemInfo) AsMap() map[string]string {
	values := map[string]string{}
	add := func(key, value string) {
		value = strings.TrimSpace(value)
		if value != "" {
			values[key] = value
		}
	}
	add("gpuVendor", info.GPUVendor)
	add("gpuModel", info.GPUModel)
	add("gpuDriver", info.GPUDriver)
	add("cpu", info.CPU)
	if info.RAMGB > 0 {
		values["ramGb"] = strconv.FormatFloat(info.RAMGB, 'f', -1, 64)
	}
	add("distro", info.Distro)
	add("kernel", info.Kernel)
	add("desktop", info.Desktop)

	if !info.HasValues() {
		return values
	}
	normalized := info.Normalized
	if normalized == (core.NormalizedSystemProfile{}) {
		normalized = info.NormalizedProfile()
	}
	add("normalized.gpuVendor", normalized.GPUVendor)
	add("normalized.gpuModel", normalized.GPUModel)
	add("normalized.gpuDriver", normalized.GPUDriver)
	add("normalized.cpuVendor", normalized.CPUVendor)
	add("normalized.cpuClass", normalized.CPUClass)
	add("normalized.ramBucket", normalized.RAMBucket)
	add("normalized.distroFamily", normalized.DistroFamily)
	add("normalized.kernel", normalized.Kernel)
	add("normalized.sessionType", normalized.SessionType)
	return values
}

func buildFTSQuery(query string) string {
	fields := strings.Fields(strings.TrimSpace(query))
	parts := make([]string, 0, len(fields))
	for _, field := range fields {
		field = strings.TrimSpace(field)
		if field == "" {
			continue
		}
		field = strings.ReplaceAll(field, `"`, `""`)
		parts = append(parts, `"`+field+`"`)
	}
	return strings.Join(parts, " AND ")
}

func normalizeLimit(limit int) int {
	if limit <= 0 {
		return 20
	}
	if limit > 100 {
		return 100
	}
	return limit
}

func formatTimestamp(t time.Time) string {
	return t.UTC().Truncate(time.Second).Format(time.RFC3339)
}

func formatDate(t time.Time) string {
	return t.UTC().Format(dateLayout)
}
