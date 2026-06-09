package advisor

import (
	"fmt"
	"math"
	"sort"
	"strings"
	"time"

	"protonsage/internal/core"
)

const (
	freshDays      = 90
	recentDays     = 365
	historicalDays = 730
)

type SimilarityResult = core.SimilarityResult
type RankedReport = core.RankedReport

// RecencyScore weights fresh reports strongly and drops reports older than a year sharply.
func RecencyScore(reportTime, now time.Time) float64 {
	if reportTime.IsZero() {
		return 0
	}
	if now.IsZero() {
		now = time.Now().UTC()
	}
	ageDays := now.Sub(reportTime).Hours() / 24
	if ageDays <= 0 {
		return 1
	}

	switch {
	case ageDays <= 30:
		return 1
	case ageDays <= freshDays:
		return roundScore(0.95 - ((ageDays-30)/(freshDays-30))*0.10)
	case ageDays < recentDays:
		return roundScore(0.75 - ((ageDays-freshDays)/(recentDays-freshDays))*0.35)
	case ageDays < historicalDays:
		return roundScore(0.24 - ((ageDays-recentDays)/(historicalDays-recentDays))*0.12)
	default:
		return 0.05
	}
}

// FreshnessLabel classifies reports for UI/CLI stale warnings.
func FreshnessLabel(reportTime, now time.Time) string {
	age := ReportAgeDays(reportTime, now)
	switch {
	case age < freshDays:
		return core.FreshnessFresh
	case age < recentDays:
		return core.FreshnessRecent
	case age < historicalDays:
		return core.FreshnessStale
	default:
		return core.FreshnessHistorical
	}
}

// ReportAgeDays returns whole report age in days, clamped to zero for future timestamps.
func ReportAgeDays(reportTime, now time.Time) int {
	if reportTime.IsZero() {
		return 0
	}
	if now.IsZero() {
		now = time.Now().UTC()
	}
	age := int(now.Sub(reportTime).Hours() / 24)
	if age < 0 {
		return 0
	}
	return age
}

// SystemSimilarity compares normalized local and report system profiles. Unknown fields are explanatory but neutral.
func SystemSimilarity(user, report core.NormalizedSystemProfile) SimilarityResult {
	fields := []similarityField{
		{name: "GPU vendor", user: user.GPUVendor, report: report.GPUVendor, weight: 0.28},
		{name: "GPU model", user: user.GPUModel, report: report.GPUModel, weight: 0.12, partial: true},
		{name: "GPU driver", user: user.GPUDriver, report: report.GPUDriver, weight: 0.10},
		{name: "distro family", user: user.DistroFamily, report: report.DistroFamily, weight: 0.15},
		{name: "kernel", user: user.Kernel, report: report.Kernel, weight: 0.10},
		{name: "RAM bucket", user: user.RAMBucket, report: report.RAMBucket, weight: 0.08},
		{name: "session type", user: user.SessionType, report: report.SessionType, weight: 0.07},
		{name: "CPU vendor", user: user.CPUVendor, report: report.CPUVendor, weight: 0.05},
		{name: "CPU class", user: user.CPUClass, report: report.CPUClass, weight: 0.05},
	}

	var matchedWeight, comparableWeight float64
	result := SimilarityResult{}
	for _, field := range fields {
		userValue := normalizeComparableValue(field.user)
		reportValue := normalizeComparableValue(field.report)
		if userValue == "" || reportValue == "" {
			result.Unknowns = append(result.Unknowns, unknownFieldExplanation(field.name, userValue, reportValue))
			continue
		}
		comparableWeight += field.weight
		matchScore := fieldMatchScore(userValue, reportValue, field.partial)
		if matchScore > 0 {
			matchedWeight += field.weight * matchScore
			result.Matches = append(result.Matches, fmt.Sprintf("%s %s", strings.ToLower(field.name), reportValue))
		} else {
			result.Mismatches = append(result.Mismatches, fmt.Sprintf("%s user=%s report=%s", strings.ToLower(field.name), userValue, reportValue))
		}
	}

	if comparableWeight == 0 {
		result.Score = 0.5
		result.Summary = "No comparable system fields were available."
		return result
	}
	result.Score = roundScore(matchedWeight / comparableWeight)
	result.Summary = similaritySummary(result)
	return result
}

// RankReports returns reports ordered by deterministic recency-first advisor score.
func RankReports(reports []core.Report, profile core.SystemProfile, now time.Time) []RankedReport {
	if now.IsZero() {
		now = time.Now().UTC()
	}
	userProfile := profile.Normalized
	if normalizedProfileIsZero(userProfile) {
		userProfile = core.NormalizeSystemProfile(profile)
	}

	ranked := make([]RankedReport, 0, len(reports))
	for _, report := range reports {
		recency := RecencyScore(report.Timestamp, now)
		similarity := SystemSimilarity(userProfile, normalizedReportProfile(report))
		quality := ReportQualityScore(report)
		freshness := FreshnessLabel(report.Timestamp, now)
		item := RankedReport{
			Report:           report,
			Score:            roundScore(recency*0.72 + similarity.Score*0.20 + quality*0.08),
			RecencyScore:     recency,
			SystemSimilarity: similarity.Score,
			QualityScore:     quality,
			Freshness:        freshness,
			AgeDays:          ReportAgeDays(report.Timestamp, now),
			Similarity:       similarity,
			Reasons:          rankReasons(report, freshness, recency, similarity, quality, now),
		}
		ranked = append(ranked, item)
	}

	sort.SliceStable(ranked, func(i, j int) bool {
		if ranked[i].Score == ranked[j].Score {
			if ranked[i].Report.Timestamp.Equal(ranked[j].Report.Timestamp) {
				return reportStableID(ranked[i].Report) < reportStableID(ranked[j].Report)
			}
			return ranked[i].Report.Timestamp.After(ranked[j].Report.Timestamp)
		}
		return ranked[i].Score > ranked[j].Score
	})
	return ranked
}

// ReportQualityScore gives a small boost to positive, concrete reports without overpowering recency.
func ReportQualityScore(report core.Report) float64 {
	score := 0.45
	switch strings.ToLower(strings.TrimSpace(report.Rating)) {
	case "platinum":
		score = 0.95
	case "gold":
		score = 0.85
	case "silver":
		score = 0.65
	case "bronze":
		score = 0.45
	case "borked":
		score = 0.20
	}
	if strings.TrimSpace(report.LaunchOptions) != "" || textMentionsConcreteTweak(report.Notes) {
		score += 0.08
	}
	if strings.TrimSpace(report.Verdict) != "" {
		score += 0.03
	}
	return roundScore(clamp(score, 0, 1))
}

type similarityField struct {
	name    string
	user    string
	report  string
	weight  float64
	partial bool
}

func fieldMatchScore(user, report string, partial bool) float64 {
	if user == report {
		return 1
	}
	if partial && len(user) >= 4 && len(report) >= 4 && (strings.Contains(user, report) || strings.Contains(report, user)) {
		return 0.7
	}
	return 0
}

func normalizeComparableValue(value string) string {
	value = strings.TrimSpace(strings.ToLower(value))
	if value == "" || value == "unknown" {
		return ""
	}
	return value
}

func unknownFieldExplanation(name, userValue, reportValue string) string {
	name = strings.ToLower(name)
	switch {
	case userValue == "" && reportValue == "":
		return name + " unknown for user and report"
	case userValue == "":
		return name + " unknown for user"
	default:
		return name + " unknown for report"
	}
}

func similaritySummary(result SimilarityResult) string {
	parts := []string{}
	if len(result.Matches) > 0 {
		parts = append(parts, "matches "+strings.Join(limitStrings(result.Matches, 3), ", "))
	}
	if len(result.Mismatches) > 0 {
		parts = append(parts, "differs on "+strings.Join(limitStrings(result.Mismatches, 3), ", "))
	}
	if len(result.Unknowns) > 0 {
		parts = append(parts, "unknown "+strings.Join(limitStrings(result.Unknowns, 2), ", "))
	}
	if len(parts) == 0 {
		return "No comparable system fields were available."
	}
	return strings.Join(parts, "; ") + "."
}

func rankReasons(report core.Report, freshness string, recency float64, similarity SimilarityResult, quality float64, now time.Time) []string {
	reasons := []string{
		fmt.Sprintf("%s report (%d days old, recency %.2f)", freshness, ReportAgeDays(report.Timestamp, now), recency),
		fmt.Sprintf("system similarity %.2f: %s", similarity.Score, similarity.Summary),
		fmt.Sprintf("quality %.2f from rating/verdict/concrete tweak signals", quality),
	}
	if strings.TrimSpace(report.LaunchOptions) != "" {
		reasons = append(reasons, "explicit launch options field present")
	}
	if freshness == core.FreshnessStale || freshness == core.FreshnessHistorical {
		reasons = append(reasons, "treat as historical context, not current truth")
	}
	return reasons
}

func normalizedReportProfile(report core.Report) core.NormalizedSystemProfile {
	profile := core.NormalizeSystemInfoMap(report.SystemInfo)
	if report.SystemInfo == nil {
		return profile
	}
	apply := func(target *string, keys ...string) {
		for _, key := range keys {
			if value := strings.TrimSpace(report.SystemInfo[key]); value != "" {
				*target = strings.ToLower(value)
				return
			}
		}
		for _, key := range keys {
			for actualKey, value := range report.SystemInfo {
				if strings.EqualFold(actualKey, key) && strings.TrimSpace(value) != "" {
					*target = strings.ToLower(strings.TrimSpace(value))
					return
				}
			}
		}
	}
	apply(&profile.GPUVendor, "normalized.gpuVendor")
	apply(&profile.GPUModel, "normalized.gpuModel")
	apply(&profile.GPUDriver, "normalized.gpuDriver")
	apply(&profile.CPUVendor, "normalized.cpuVendor")
	apply(&profile.CPUClass, "normalized.cpuClass")
	apply(&profile.RAMBucket, "normalized.ramBucket")
	apply(&profile.DistroFamily, "normalized.distroFamily")
	apply(&profile.Kernel, "normalized.kernel")
	apply(&profile.SessionType, "normalized.sessionType")
	return profile
}

func normalizedProfileIsZero(profile core.NormalizedSystemProfile) bool {
	return profile == (core.NormalizedSystemProfile{})
}

func reportStableID(report core.Report) string {
	if report.SourceReportID != "" {
		return report.SourceReportID
	}
	if report.ID != 0 {
		return fmt.Sprintf("%d", report.ID)
	}
	return fmt.Sprintf("%d:%s", report.AppID, report.Timestamp.UTC().Format(time.RFC3339))
}

func limitStrings(values []string, limit int) []string {
	if len(values) <= limit {
		return values
	}
	out := append([]string{}, values[:limit]...)
	out = append(out, fmt.Sprintf("+%d more", len(values)-limit))
	return out
}

func roundScore(value float64) float64 {
	return math.Round(value*1000) / 1000
}

func clamp(value, min, max float64) float64 {
	if value < min {
		return min
	}
	if value > max {
		return max
	}
	return value
}
