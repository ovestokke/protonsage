package advisor

import (
	"fmt"
	"strings"
	"time"

	"protonsage/internal/core"
)

// GenerateRecommendation builds the deterministic no-AI recommendation for one selected game/appid.
func GenerateRecommendation(game core.Game, reports []core.Report, profile core.SystemProfile, now time.Time) core.Recommendation {
	if now.IsZero() {
		now = time.Now().UTC()
	}
	ranked := RankReports(reports, profile, now)
	suggestions := ExtractSuggestions(ranked)
	warnings := recommendationWarnings(ranked)
	return core.Recommendation{
		Game:          game,
		Summary:       recommendationSummary(game, ranked, suggestions, warnings),
		Suggestions:   suggestions,
		Citations:     collectRecommendationCitations(suggestions, ranked),
		RankedReports: ranked,
		Warnings:      warnings,
		GeneratedBy:   core.RecommendationSourceRules,
	}
}

func recommendationSummary(game core.Game, ranked []core.RankedReport, suggestions []core.Suggestion, warnings []string) string {
	name := strings.TrimSpace(game.Name)
	if name == "" {
		name = fmt.Sprintf("appid %d", game.AppID)
	}
	if len(ranked) == 0 {
		return fmt.Sprintf("No imported ProtonDB reports were found for %s; import ProtonDB data before generating cited recommendations.", name)
	}

	top := ranked[0]
	topDate := "unknown date"
	if !top.Report.Timestamp.IsZero() {
		topDate = top.Report.Timestamp.UTC().Format("2006-01-02")
	}

	action := "No copyable launch option was extracted; review the cited report notes before changing settings."
	if best, ok := firstCopyableSuggestion(suggestions); ok {
		action = fmt.Sprintf("Start with %s suggestion %q (%s confidence).", strings.ReplaceAll(best.Kind, "_", " "), best.Snippet, best.Confidence)
	} else if len(suggestions) > 0 {
		action = "No copyable launch option was extracted; review the cited note/diagnostic suggestions before changing settings."
	}

	freshness := fmt.Sprintf("Top evidence is a %s report from %s", top.Freshness, topDate)
	if top.Report.Rating != "" {
		freshness += " rated " + top.Report.Rating
	}
	freshness += fmt.Sprintf(" with %.2f system similarity", top.SystemSimilarity)

	uncertainty := ""
	if top.Freshness == core.FreshnessStale || top.Freshness == core.FreshnessHistorical {
		uncertainty = " Latest evidence is stale, so treat it as historical context."
	} else if olderSimilarReportExists(ranked, top) {
		uncertainty = " Older reports with closer hardware matches remain historical context; recency wins."
	} else if top.SystemSimilarity < 0.45 {
		uncertainty = " System mismatch lowers confidence; prefer suggestions with multiple fresh citations."
	}
	if len(warnings) > 0 && uncertainty == "" {
		uncertainty = " " + warnings[0]
	}

	return action + " " + freshness + "." + uncertainty
}

func firstCopyableSuggestion(suggestions []core.Suggestion) (core.Suggestion, bool) {
	for _, suggestion := range suggestions {
		if isCopyableSuggestion(suggestion) {
			return suggestion, true
		}
	}
	return core.Suggestion{}, false
}

func recommendationWarnings(ranked []core.RankedReport) []string {
	if len(ranked) == 0 {
		return nil
	}
	var stale, historical int
	for _, report := range ranked {
		switch report.Freshness {
		case core.FreshnessHistorical:
			historical++
			stale++
		case core.FreshnessStale:
			stale++
		}
	}
	var warnings []string
	if stale == len(ranked) {
		warnings = append(warnings, "All imported reports for this appid are stale or historical; do not treat them as current truth.")
	} else if stale > 0 {
		warnings = append(warnings, fmt.Sprintf("%d stale/historical reports are included only as context.", stale))
	}
	if historical > 0 {
		warnings = append(warnings, fmt.Sprintf("%d reports are older than two years and marked historical.", historical))
	}
	return warnings
}

func olderSimilarReportExists(ranked []core.RankedReport, top core.RankedReport) bool {
	for _, report := range ranked[1:] {
		if report.SystemSimilarity > top.SystemSimilarity+0.25 && (report.Freshness == core.FreshnessStale || report.Freshness == core.FreshnessHistorical) {
			return true
		}
	}
	return false
}

func collectRecommendationCitations(suggestions []core.Suggestion, ranked []core.RankedReport) []core.Citation {
	seen := map[string]bool{}
	var citations []core.Citation
	add := func(citation core.Citation) {
		key := citation.SourceID + "\x00" + citation.ReportID + "\x00" + citation.Snippet
		if citation.SourceID == "" && citation.ReportID == "" && citation.Snippet == "" {
			return
		}
		if seen[key] {
			return
		}
		seen[key] = true
		citations = append(citations, citation)
	}
	for _, suggestion := range suggestions {
		for _, citation := range suggestion.Sources {
			add(citation)
		}
	}
	if len(citations) > 0 {
		return citations
	}
	for i, report := range ranked {
		if i >= 5 {
			break
		}
		add(citationForReport(report.Report, reportCitationSnippet(report.Report)))
	}
	return citations
}

func reportCitationSnippet(report core.Report) string {
	for _, value := range []string{report.LaunchOptions, report.Verdict, report.Notes} {
		value = strings.TrimSpace(value)
		if value == "" {
			continue
		}
		if len(value) > 180 {
			return value[:180] + "…"
		}
		return value
	}
	return "ProtonDB report"
}
