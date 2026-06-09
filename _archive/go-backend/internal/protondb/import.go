package protondb

import (
	"archive/tar"
	"bytes"
	"compress/gzip"
	"context"
	"crypto/sha1"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"

	"protonsage/internal/core"
	"protonsage/internal/storage"
)

const (
	// DataLicenseNote is stored with every import to preserve ProtonDB attribution.
	DataLicenseNote = "ProtonDB/protondb-data report data; ODbL/DbCL attribution: https://github.com/bdefore/protondb-data"
	reportsJSONName = "reports_piiremoved.json"
)

// SnapshotImportMeta describes the local ProtonDB snapshot/fixture being imported.
type SnapshotImportMeta struct {
	SourceID         string
	SnapshotFilename string
	SnapshotDate     time.Time
	SourceURL        string
	LicenseNote      string
}

// ImportResult summarizes one fixture/snapshot import.
type ImportResult struct {
	ImportRunID        int64  `json:"importRunId"`
	SourceID           string `json:"sourceId"`
	SnapshotFilename   string `json:"snapshotFilename"`
	GamesImported      int    `json:"gamesImported"`
	ReportsImported    int    `json:"reportsImported"`
	SystemInfoImported int    `json:"systemInfoImported"`
	RecordsSkipped     int    `json:"recordsSkipped"`
}

type rawReportRecord struct {
	AppID          int
	Title          string
	SourceReportID string
	Fields         map[string]any
	RawJSON        string
}

type normalizedReport struct {
	Game           core.Game
	Report         core.Report
	SourceReportID string
	RawJSON        string
	SystemInfo     storage.ReportSystemInfo
	HasSystemInfo  bool
}

// ImportSnapshot imports a modern ProtonDB reports_*.tar.gz archive from r.
// It expects the archive to contain reports_piiremoved.json and performs all
// storage writes inside one SQLite transaction.
func ImportSnapshot(ctx context.Context, db *storage.DB, r io.Reader, meta SnapshotImportMeta) (ImportResult, error) {
	if db == nil {
		return ImportResult{}, errors.New("import snapshot: storage db is required")
	}
	if r == nil {
		return ImportResult{}, errors.New("import snapshot: reader is required")
	}
	meta, err := normalizeImportMeta(meta)
	if err != nil {
		return ImportResult{}, err
	}

	payload, err := readReportsJSON(r)
	if err != nil {
		return ImportResult{}, err
	}
	var root any
	decoder := json.NewDecoder(bytes.NewReader(payload))
	decoder.UseNumber()
	if err := decoder.Decode(&root); err != nil {
		return ImportResult{}, fmt.Errorf("decode %s: %w", reportsJSONName, err)
	}

	rawRecords := collectRawReports(root)
	if len(rawRecords) == 0 {
		return ImportResult{}, fmt.Errorf("decode %s: no report records found", reportsJSONName)
	}

	result := ImportResult{
		SourceID:         meta.SourceID,
		SnapshotFilename: meta.SnapshotFilename,
	}
	seenGames := map[int]bool{}

	err = db.WithTx(ctx, func(q *storage.Queries) error {
		now := time.Now().UTC()
		if err := q.UpsertSource(ctx, storage.SourceInput{
			ID:         meta.SourceID,
			Kind:       "protondb-data",
			URL:        meta.SourceURL,
			License:    meta.LicenseNote,
			ImportedAt: now,
		}); err != nil {
			return err
		}

		runID, err := q.CreateImportRun(ctx, storage.ImportRunInput{
			SourceID:         meta.SourceID,
			SnapshotFilename: meta.SnapshotFilename,
			SnapshotDate:     meta.SnapshotDate,
			SourceURL:        meta.SourceURL,
			License:          meta.LicenseNote,
			StartedAt:        now,
		})
		if err != nil {
			return err
		}
		result.ImportRunID = runID

		for i, raw := range rawRecords {
			select {
			case <-ctx.Done():
				return ctx.Err()
			default:
			}

			normalized, ok := normalizeReport(raw, meta.SourceID, i)
			if !ok {
				result.RecordsSkipped++
				continue
			}
			if err := q.UpsertGame(ctx, normalized.Game); err != nil {
				return err
			}
			if !seenGames[normalized.Game.AppID] {
				seenGames[normalized.Game.AppID] = true
				result.GamesImported++
			}

			reportID, err := q.InsertReport(ctx, storage.ReportInput{
				ImportRunID:    runID,
				SourceReportID: normalized.SourceReportID,
				RawJSON:        normalized.RawJSON,
				Report:         normalized.Report,
			})
			if err != nil {
				return err
			}
			result.ReportsImported++

			if normalized.HasSystemInfo {
				if err := q.UpsertReportSystemInfo(ctx, reportID, normalized.SystemInfo); err != nil {
					return err
				}
				result.SystemInfoImported++
			}
		}

		return q.FinishImportRun(ctx, runID, result.ReportsImported, result.RecordsSkipped)
	})
	if err != nil {
		return result, err
	}
	if result.ReportsImported == 0 {
		return result, fmt.Errorf("import snapshot: no valid reports imported; skipped %d records", result.RecordsSkipped)
	}
	return result, nil
}

func normalizeImportMeta(meta SnapshotImportMeta) (SnapshotImportMeta, error) {
	meta.SourceID = strings.TrimSpace(meta.SourceID)
	meta.SnapshotFilename = strings.TrimSpace(meta.SnapshotFilename)
	meta.SourceURL = strings.TrimSpace(meta.SourceURL)
	meta.LicenseNote = strings.TrimSpace(meta.LicenseNote)
	if meta.SnapshotFilename == "" {
		return SnapshotImportMeta{}, errors.New("import snapshot: snapshot filename is required")
	}
	if meta.SnapshotDate.IsZero() {
		if parsed, ok := ParseSnapshotFilename(meta.SnapshotFilename); ok {
			meta.SnapshotDate = parsed
		} else {
			return SnapshotImportMeta{}, errors.New("import snapshot: snapshot date is required when filename date cannot be parsed")
		}
	}
	if meta.SourceURL == "" {
		meta.SourceURL = ReportsRawBaseURL + "/" + meta.SnapshotFilename
	}
	if meta.LicenseNote == "" {
		meta.LicenseNote = DataLicenseNote
	}
	if meta.SourceID == "" {
		meta.SourceID = "protondb-data:" + meta.SnapshotFilename
	}
	return meta, nil
}

func readReportsJSON(r io.Reader) ([]byte, error) {
	gz, err := gzip.NewReader(r)
	if err != nil {
		return nil, fmt.Errorf("open snapshot gzip stream: %w", err)
	}
	defer gz.Close()

	tr := tar.NewReader(gz)
	for {
		header, err := tr.Next()
		if errors.Is(err, io.EOF) {
			break
		}
		if err != nil {
			return nil, fmt.Errorf("read snapshot tar stream: %w", err)
		}
		if header.Typeflag == tar.TypeDir {
			continue
		}
		if filepath.Base(header.Name) != reportsJSONName {
			continue
		}
		payload, err := io.ReadAll(tr)
		if err != nil {
			return nil, fmt.Errorf("read %s from snapshot: %w", reportsJSONName, err)
		}
		return payload, nil
	}
	return nil, fmt.Errorf("snapshot archive does not contain %s", reportsJSONName)
}

func collectRawReports(root any) []rawReportRecord {
	var records []rawReportRecord
	collectValue(&records, root, 0, "", "")
	return records
}

func collectValue(records *[]rawReportRecord, value any, inheritedAppID int, inheritedTitle, inheritedID string) {
	switch typed := value.(type) {
	case []any:
		for i, item := range typed {
			itemID := joinID(inheritedID, strconv.Itoa(i))
			collectValue(records, item, inheritedAppID, inheritedTitle, itemID)
		}
	case map[string]any:
		appid := inheritedAppID
		if explicitAppID, ok := reportAppID(typed); ok {
			appid = explicitAppID
		}
		title := inheritedTitle
		if explicitTitle := reportTitle(typed); explicitTitle != "" {
			title = explicitTitle
		}

		if reports, ok := firstValue(typed, "reports"); ok {
			for _, item := range reportItems(reports) {
				collectValue(records, item.value, appid, title, joinID(inheritedID, item.id))
			}
			return
		}

		if looksLikeReport(typed) || appid > 0 {
			id := firstString(typed, "id", "reportId", "reportID", "uuid", "_id")
			if id == "" {
				id = inheritedID
			}
			*records = append(*records, rawReportRecord{
				AppID:          appid,
				Title:          title,
				SourceReportID: id,
				Fields:         typed,
				RawJSON:        mustMarshalString(typed),
			})
			return
		}

		keys := sortedKeys(typed)
		for _, key := range keys {
			keyAppID := appid
			if parsed, ok := parseIntString(key); ok {
				keyAppID = parsed
			}
			collectValue(records, typed[key], keyAppID, title, joinID(inheritedID, key))
		}
	}
}

type reportItem struct {
	id    string
	value any
}

func reportItems(value any) []reportItem {
	switch typed := value.(type) {
	case []any:
		items := make([]reportItem, 0, len(typed))
		for i, item := range typed {
			items = append(items, reportItem{id: strconv.Itoa(i), value: item})
		}
		return items
	case map[string]any:
		keys := sortedKeys(typed)
		items := make([]reportItem, 0, len(keys))
		for _, key := range keys {
			items = append(items, reportItem{id: key, value: typed[key]})
		}
		return items
	default:
		return nil
	}
}

func looksLikeReport(fields map[string]any) bool {
	if _, ok := firstValue(fields, "timestamp", "createdAt", "created_at", "date", "time"); ok {
		return true
	}
	if firstString(fields, "notes", "text", "body", "content", "report") != "" {
		return true
	}
	if firstString(fields, "rating", "verdict", "tier") != "" {
		return true
	}
	return false
}

func normalizeReport(raw rawReportRecord, sourceID string, index int) (normalizedReport, bool) {
	fields := raw.Fields
	appid := raw.AppID
	if explicitAppID, ok := reportAppID(fields); ok {
		appid = explicitAppID
	}
	if appid <= 0 {
		return normalizedReport{}, false
	}

	timestampValue, ok := firstValue(fields, "timestamp", "createdAt", "created_at", "date", "time")
	if !ok {
		return normalizedReport{}, false
	}
	timestamp, ok := parseReportTime(timestampValue)
	if !ok {
		return normalizedReport{}, false
	}

	title := firstNonEmpty(
		reportTitle(fields),
		raw.Title,
		"Steam App "+strconv.Itoa(appid),
	)
	sourceReportID := firstNonEmpty(
		firstString(fields, "id", "reportId", "reportID", "uuid", "_id"),
		raw.SourceReportID,
	)
	if sourceReportID == "" || parseIntOnly(sourceReportID) {
		sourceReportID = generatedReportID(sourceID, appid, timestamp, index, raw.RawJSON)
	}

	systemValue, hasSystemInfo := firstValue(fields, "systemInfo", "system", "specs")
	systemRaw := ""
	if hasSystemInfo {
		systemRaw = mustMarshalString(systemValue)
	}
	info := normalizeSystemInfo(systemValue, fields)
	info.RawJSON = systemRaw
	if !hasSystemInfo && !systemInfoHasValues(info) {
		info = storage.ReportSystemInfo{}
	}

	report := core.Report{
		SourceReportID: sourceReportID,
		AppID:          appid,
		Title:          title,
		Timestamp:      timestamp,
		Verdict:        reportVerdict(fields),
		Rating:         reportRating(fields),
		Notes:          reportNotes(fields),
		LaunchOptions:  reportLaunchOptions(fields),
		ProtonVersion:  reportProtonVersion(fields),
		SourceID:       sourceID,
	}
	report.SystemInfo = info.AsMap()

	return normalizedReport{
		Game: core.Game{
			AppID:    appid,
			Name:     title,
			Launcher: core.LauncherSteam,
		},
		Report:         report,
		SourceReportID: sourceReportID,
		RawJSON:        firstNonEmpty(raw.RawJSON, "{}"),
		SystemInfo:     info,
		HasSystemInfo:  hasSystemInfo || systemInfoHasValues(info),
	}, true
}

func reportAppID(fields map[string]any) (int, bool) {
	if explicitAppID, ok := firstInt(fields, "appid", "appId", "steamAppId", "steam_appid"); ok {
		return explicitAppID, true
	}
	if appFields, ok := firstValueMap(fields, "app"); ok {
		if explicitAppID, ok := firstInt(appFields, "appid", "appId", "steamAppId", "steam_appid"); ok {
			return explicitAppID, true
		}
		if steamFields, ok := firstValueMap(appFields, "steam"); ok {
			if explicitAppID, ok := firstInt(steamFields, "appid", "appId", "steamAppId", "steam_appid"); ok {
				return explicitAppID, true
			}
		}
	}
	if responseFields, ok := firstValueMap(fields, "responses"); ok {
		if explicitAppID, ok := firstInt(responseFields, "answerToWhatGame", "appid", "appId", "steamAppId", "steam_appid"); ok {
			return explicitAppID, true
		}
	}
	return 0, false
}

func reportTitle(fields map[string]any) string {
	if title := firstString(fields, "title", "name", "gameTitle", "game_name"); title != "" {
		return title
	}
	if appFields, ok := firstValueMap(fields, "app"); ok {
		return firstString(appFields, "title", "name", "gameTitle", "game_name")
	}
	return ""
}

func reportVerdict(fields map[string]any) string {
	if verdict := firstString(fields, "verdict", "summary"); verdict != "" {
		return verdict
	}
	if responseFields, ok := firstValueMap(fields, "responses"); ok {
		return firstString(responseFields, "verdict", "summary")
	}
	return ""
}

func reportRating(fields map[string]any) string {
	if rating := firstString(fields, "rating", "tier"); rating != "" {
		return rating
	}
	if responseFields, ok := firstValueMap(fields, "responses"); ok {
		return firstString(responseFields, "rating", "tier", "verdict")
	}
	return ""
}

func reportLaunchOptions(fields map[string]any) string {
	if launchOptions := firstString(fields, "launchOptions", "launch_options", "launchOption", "launch_option"); launchOptions != "" {
		return launchOptions
	}
	if responseFields, ok := firstValueMap(fields, "responses"); ok {
		return firstString(responseFields, "launchOptions", "launch_options", "launchOption", "launch_option")
	}
	return ""
}

func reportProtonVersion(fields map[string]any) string {
	if protonVersion := firstString(fields, "protonVersion", "proton_version", "proton", "protonVersionName"); protonVersion != "" {
		return protonVersion
	}
	if responseFields, ok := firstValueMap(fields, "responses"); ok {
		return firstString(responseFields, "protonVersion", "proton_version", "proton", "protonVersionName")
	}
	return ""
}

func reportNotes(fields map[string]any) string {
	var parts []string
	if value, ok := firstValue(fields, "notes", "text", "body", "content", "report"); ok {
		parts = append(parts, noteTextFromValue(value)...)
	}
	for _, key := range noteResponseKeys() {
		if text := firstString(fields, key); looksLikeFreeformNote(text) {
			parts = append(parts, key+": "+text)
		}
	}
	if responseFields, ok := firstValueMap(fields, "responses"); ok {
		if value, ok := firstValue(responseFields, "notes"); ok {
			parts = append(parts, noteTextFromValue(value)...)
		}
		for _, key := range noteResponseKeys() {
			if text := firstString(responseFields, key); looksLikeFreeformNote(text) {
				parts = append(parts, key+": "+text)
			}
		}
	}
	return strings.Join(dedupeStrings(parts), "\n")
}

func noteResponseKeys() []string {
	return []string{"extra", "customizationsUsed", "audioFaults", "graphicalFaults", "inputFaults", "performanceFaults", "saveGameFaults", "significantBugs", "stabilityFaults", "windowingFaults"}
}

func noteTextFromValue(value any) []string {
	switch typed := value.(type) {
	case nil:
		return nil
	case string:
		if looksLikeFreeformNote(typed) {
			return []string{strings.TrimSpace(typed)}
		}
		return nil
	case map[string]any:
		keys := sortedKeys(typed)
		var parts []string
		for _, key := range keys {
			for _, text := range noteTextFromValue(typed[key]) {
				parts = append(parts, key+": "+text)
			}
		}
		return parts
	case []any:
		var parts []string
		for _, item := range typed {
			parts = append(parts, noteTextFromValue(item)...)
		}
		return parts
	default:
		return nil
	}
}

func looksLikeFreeformNote(value string) bool {
	value = strings.TrimSpace(value)
	if value == "" {
		return false
	}
	switch strings.ToLower(value) {
	case "yes", "no", "n/a", "na", "none", "default", "true", "false":
		return false
	}
	return len(value) > 2
}

func normalizeSystemInfo(systemValue any, reportFields map[string]any) storage.ReportSystemInfo {
	fields, _ := systemValue.(map[string]any)
	if fields == nil {
		fields = map[string]any{}
	}
	gpuFields, _ := firstValueMap(fields, "gpu", "graphics", "videoCard", "video_card")
	gpuText := firstNonEmpty(firstString(fields, "gpu", "graphics", "videoCard", "video_card"), firstString(reportFields, "gpu"))
	gpuVendor := firstNonEmpty(
		firstString(fields, "gpuVendor", "gpu_vendor", "vendor"),
		firstString(gpuFields, "vendor", "gpuVendor", "gpu_vendor"),
		firstString(reportFields, "gpuVendor", "gpu_vendor"),
		detectGPUVendor(gpuText),
	)
	gpuModel := firstNonEmpty(
		firstString(fields, "gpuModel", "gpu_model", "gpuName", "gpu_name", "name", "model"),
		firstString(gpuFields, "name", "model", "gpuName", "gpu_model"),
		firstString(reportFields, "gpuModel", "gpu_model", "gpuName", "gpu_name"),
		gpuText,
	)
	gpuDriver := firstNonEmpty(
		firstString(fields, "gpuDriver", "gpu_driver", "driver", "driverVersion", "driver_version"),
		firstString(gpuFields, "driver", "driverVersion", "driver_version"),
		firstString(reportFields, "gpuDriver", "gpu_driver", "driver", "driverVersion", "driver_version"),
	)
	return storage.ReportSystemInfo{
		GPUVendor: gpuVendor,
		GPUModel:  gpuModel,
		GPUDriver: gpuDriver,
		CPU:       firstNonEmpty(firstString(fields, "cpu", "cpuModel", "cpu_model", "processor"), firstString(reportFields, "cpu", "cpuModel", "cpu_model")),
		RAMGB:     firstNonZeroFloat(firstFloat(fields, "ramGb", "ramGB", "ram_gb", "ram", "memory"), firstFloat(reportFields, "ramGb", "ramGB", "ram_gb")),
		Distro:    firstNonEmpty(firstString(fields, "distro", "distribution", "os", "osName", "os_name"), firstString(reportFields, "distro", "distribution", "os")),
		Kernel:    firstNonEmpty(firstString(fields, "kernel", "kernelVersion", "kernel_version"), firstString(reportFields, "kernel", "kernelVersion", "kernel_version")),
		Desktop:   firstNonEmpty(firstString(fields, "desktop", "desktopEnvironment", "desktop_environment", "session", "sessionType"), firstString(reportFields, "desktop", "desktopEnvironment", "session")),
	}
}

func systemInfoHasValues(info storage.ReportSystemInfo) bool {
	return info.GPUVendor != "" || info.GPUModel != "" || info.GPUDriver != "" || info.CPU != "" || info.RAMGB > 0 || info.Distro != "" || info.Kernel != "" || info.Desktop != ""
}

func parseReportTime(value any) (time.Time, bool) {
	switch typed := value.(type) {
	case string:
		return parseReportTimeString(typed)
	case json.Number:
		return parseUnixNumber(typed.String())
	case float64:
		return parseUnixFloat(typed)
	case int64:
		return time.Unix(typed, 0).UTC(), true
	case int:
		return time.Unix(int64(typed), 0).UTC(), true
	default:
		return time.Time{}, false
	}
}

func parseReportTimeString(value string) (time.Time, bool) {
	value = strings.TrimSpace(value)
	if value == "" {
		return time.Time{}, false
	}
	if t, ok := parseUnixNumber(value); ok {
		return t, true
	}
	layouts := []string{
		time.RFC3339Nano,
		time.RFC3339,
		"2006-01-02T15:04:05.000Z0700",
		"2006-01-02T15:04:05Z0700",
		"2006-01-02 15:04:05",
		"2006-01-02",
	}
	for _, layout := range layouts {
		if t, err := time.Parse(layout, value); err == nil {
			return t.UTC(), true
		}
	}
	return time.Time{}, false
}

func parseUnixNumber(value string) (time.Time, bool) {
	value = strings.TrimSpace(value)
	if value == "" {
		return time.Time{}, false
	}
	seconds, err := strconv.ParseFloat(value, 64)
	if err != nil {
		return time.Time{}, false
	}
	return parseUnixFloat(seconds)
}

func parseUnixFloat(value float64) (time.Time, bool) {
	if value <= 0 {
		return time.Time{}, false
	}
	// Treat very large numeric timestamps as milliseconds since Unix epoch.
	if value > 1_000_000_000_000 {
		value = value / 1000
	}
	sec := int64(value)
	nsec := int64((value - float64(sec)) * 1_000_000_000)
	return time.Unix(sec, nsec).UTC(), true
}

func firstValue(fields map[string]any, keys ...string) (any, bool) {
	if fields == nil {
		return nil, false
	}
	for _, key := range keys {
		if value, ok := fields[key]; ok {
			return value, true
		}
	}
	for _, key := range keys {
		for actualKey, value := range fields {
			if strings.EqualFold(actualKey, key) {
				return value, true
			}
		}
	}
	return nil, false
}

func firstValueMap(fields map[string]any, keys ...string) (map[string]any, bool) {
	value, ok := firstValue(fields, keys...)
	if !ok {
		return nil, false
	}
	mapped, ok := value.(map[string]any)
	return mapped, ok
}

func firstString(fields map[string]any, keys ...string) string {
	value, ok := firstValue(fields, keys...)
	if !ok {
		return ""
	}
	return stringFromAny(value)
}

func firstInt(fields map[string]any, keys ...string) (int, bool) {
	value, ok := firstValue(fields, keys...)
	if !ok {
		return 0, false
	}
	return intFromAny(value)
}

func firstFloat(fields map[string]any, keys ...string) float64 {
	value, ok := firstValue(fields, keys...)
	if !ok {
		return 0
	}
	float, _ := floatFromAny(value)
	return float
}

func stringFromAny(value any) string {
	switch typed := value.(type) {
	case string:
		return strings.TrimSpace(typed)
	case json.Number:
		return typed.String()
	case float64:
		return strconv.FormatFloat(typed, 'f', -1, 64)
	case bool:
		if typed {
			return "true"
		}
		return "false"
	case nil:
		return ""
	default:
		return ""
	}
}

func intFromAny(value any) (int, bool) {
	switch typed := value.(type) {
	case json.Number:
		if i, err := typed.Int64(); err == nil {
			return int(i), true
		}
		if f, err := strconv.ParseFloat(typed.String(), 64); err == nil {
			return int(f), true
		}
	case float64:
		return int(typed), true
	case string:
		return parseIntString(typed)
	case int:
		return typed, true
	case int64:
		return int(typed), true
	}
	return 0, false
}

func floatFromAny(value any) (float64, bool) {
	switch typed := value.(type) {
	case json.Number:
		f, err := strconv.ParseFloat(typed.String(), 64)
		return f, err == nil
	case float64:
		return typed, true
	case string:
		trimmed := strings.TrimSpace(strings.TrimSuffix(strings.ToLower(typed), "gb"))
		trimmed = strings.TrimSpace(trimmed)
		f, err := strconv.ParseFloat(trimmed, 64)
		return f, err == nil
	}
	return 0, false
}

func parseIntString(value string) (int, bool) {
	value = strings.TrimSpace(value)
	if value == "" {
		return 0, false
	}
	parsed, err := strconv.Atoi(value)
	if err != nil {
		return 0, false
	}
	return parsed, true
}

func parseIntOnly(value string) bool {
	_, ok := parseIntString(value)
	return ok
}

func dedupeStrings(values []string) []string {
	seen := map[string]bool{}
	out := make([]string, 0, len(values))
	for _, value := range values {
		value = strings.TrimSpace(value)
		key := strings.ToLower(value)
		if value == "" || seen[key] {
			continue
		}
		seen[key] = true
		out = append(out, value)
	}
	return out
}

func firstNonEmpty(values ...string) string {
	for _, value := range values {
		value = strings.TrimSpace(value)
		if value != "" {
			return value
		}
	}
	return ""
}

func firstNonZeroFloat(values ...float64) float64 {
	for _, value := range values {
		if value > 0 {
			return value
		}
	}
	return 0
}

func sortedKeys(m map[string]any) []string {
	keys := make([]string, 0, len(m))
	for key := range m {
		keys = append(keys, key)
	}
	sort.Strings(keys)
	return keys
}

func joinID(parts ...string) string {
	clean := make([]string, 0, len(parts))
	for _, part := range parts {
		part = strings.TrimSpace(part)
		if part != "" {
			clean = append(clean, part)
		}
	}
	return strings.Join(clean, ":")
}

func generatedReportID(sourceID string, appid int, timestamp time.Time, index int, rawJSON string) string {
	hash := sha1.Sum([]byte(fmt.Sprintf("%s\n%d\n%s\n%d\n%s", sourceID, appid, timestamp.UTC().Format(time.RFC3339), index, rawJSON)))
	return fmt.Sprintf("generated:%d:%s:%s", appid, timestamp.UTC().Format("20060102T150405Z"), hex.EncodeToString(hash[:6]))
}

func mustMarshalString(value any) string {
	payload, err := json.Marshal(value)
	if err != nil {
		return "{}"
	}
	return string(payload)
}

func detectGPUVendor(value string) string {
	lower := strings.ToLower(value)
	switch {
	case strings.Contains(lower, "nvidia") || strings.Contains(lower, "geforce") || strings.Contains(lower, "rtx") || strings.Contains(lower, "gtx"):
		return "NVIDIA"
	case strings.Contains(lower, "amd") || strings.Contains(lower, "radeon") || strings.Contains(lower, "radv"):
		return "AMD"
	case strings.Contains(lower, "intel") || strings.Contains(lower, "arc"):
		return "Intel"
	default:
		return ""
	}
}
