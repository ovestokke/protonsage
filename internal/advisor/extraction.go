package advisor

import (
	"crypto/sha1"
	"encoding/hex"
	"fmt"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"

	"protonsage/internal/core"
)

var envAssignPattern = regexp.MustCompile("(?i)\\b[A-Z_][A-Z0-9_]{2,}=(?:\"[^\"\\s;|&`<>]+\"|'[^'\\s;|&`<>]+'|[A-Za-z0-9_.,:/@%+\\-]+)")

var knownWorkaroundPatterns = []struct {
	kind    string
	pattern *regexp.Regexp
}{
	// Proton version workarounds
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\buse\s+Proton\s+Experimental\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\buse\s+(?:Proton\s+)?GE[-\s]?Proton[-\d]*\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\b(?:switch|change|set)\s+(?:the\s+)?(?:compatibility\s+)?\s*tool\s+to\s+Proton\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\buse\s+Proton\s+[\d.]+\b`)},

	// Intro/video/splash skip
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\bdisabl(?:e|ing)\s+(?:the\s+)?intro\s+videos?\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\bskip\s+(?:the\s+)?intro\s+videos?\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\bskip\s+(?:the\s+)?intro\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\bdisabl(?:e|ing)\s+(?:the\s+)?launcher\b`)},

	// Display/window issues
	{kind: core.SuggestionKindDiagnostic, pattern: regexp.MustCompile(`(?i)\bblack\s+screen\b`)},
	{kind: core.SuggestionKindDiagnostic, pattern: regexp.MustCompile(`(?i)\bwhite\s+screen\b`)},
	{kind: core.SuggestionKindDiagnostic, pattern: regexp.MustCompile(`(?i)\bscreen\s+flick(?:er|ering)\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\b(?:run|start|launch|use|play)\s+(?:in\s+)?(?:windowed|window)\s+mode\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\b(?:force|set)\s+(?:to\s+)?(?:windowed|window)\s+mode\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\bdisabl(?:e|ing)\s+(?:the\s+)?fullscreen\b`)},

	// Audio issues
	{kind: core.SuggestionKindDiagnostic, pattern: regexp.MustCompile(`(?i)\bno\s+(?:audio|sound)\b`)},
	{kind: core.SuggestionKindDiagnostic, pattern: regexp.MustCompile(`(?i)\baudio\s+(?:crackl|stutter|cut|lag)\w*\b`)},

	// Controller/input
	{kind: core.SuggestionKindDiagnostic, pattern: regexp.MustCompile(`(?i)\bcontroller\s+(?:not\s+)?(?:work|detect|respond)\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\bdisabl(?:e|ing)\s+(?:Steam\s+)?input\b`)},

	// Freezes/crashes
	{kind: core.SuggestionKindDiagnostic, pattern: regexp.MustCompile(`(?i)\b(?:freeze|hang|lock\s*up)s?\s+(?:on|at|during|after)?\b`)},
	{kind: core.SuggestionKindDiagnostic, pattern: regexp.MustCompile(`(?i)\bcrash(?:es)?\s+(?:on|at|during|after|to|upon)\b`)},
	{kind: core.SuggestionKindDiagnostic, pattern: regexp.MustCompile(`(?i)\bcrash(?:es)?\s+(?:to|on)\s+(?:desktop|lobby)\b`)},

	// Multiplayer/anti-cheat
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\banti[-\s]?cheat\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\bEAC\s+(?:enabled|support)\b`)},

	// Common tips
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\buse\s+(?:the\s+)?native\s+(?:Linux\s+)?version\b`)},
	{kind: core.SuggestionKindWorkaround, pattern: regexp.MustCompile(`(?i)\b(?:install|use)\s+(?:protontricks|winetricks)\b`)},
	{kind: core.SuggestionKindNote, pattern: regexp.MustCompile(`(?i)\bno\s+tweaks?\s+required\b`)},
	{kind: core.SuggestionKindNote, pattern: regexp.MustCompile(`(?i)\b(?:works|runs)\s+(?:out\s+of\s+the\s+box|perfectly|flawlessly)\b`)},
}

type extractionCandidate struct {
	snippet string
	kind    string
	ranked  core.RankedReport
}

type suggestionGroup struct {
	snippet       string
	kind          string
	sources       []core.Citation
	sourceSeen    map[string]bool
	recencyScores []float64
	simScores     []float64
}

// ExtractSuggestions converts ranked reports into grouped, cited, selectable suggestions.
func ExtractSuggestions(ranked []core.RankedReport) []core.Suggestion {
	groups := map[string]*suggestionGroup{}
	for _, report := range ranked {
		for _, candidate := range extractCandidates(report) {
			candidate.snippet = cleanSnippet(candidate.snippet)
			if candidate.snippet == "" || isDangerousSnippet(candidate.snippet) {
				continue
			}
			key := candidate.kind + "\x00" + canonicalSnippet(candidate.snippet)
			group := groups[key]
			if group == nil {
				group = &suggestionGroup{
					snippet:    candidate.snippet,
					kind:       candidate.kind,
					sourceSeen: map[string]bool{},
				}
				groups[key] = group
			}
			citation := citationForReport(candidate.ranked.Report, candidate.snippet)
			sourceKey := citation.SourceID + "\x00" + citation.ReportID + "\x00" + canonicalSnippet(candidate.snippet)
			if !group.sourceSeen[sourceKey] {
				group.sourceSeen[sourceKey] = true
				group.sources = append(group.sources, citation)
				group.recencyScores = append(group.recencyScores, candidate.ranked.RecencyScore)
				group.simScores = append(group.simScores, candidate.ranked.SystemSimilarity)
			}
		}
	}

	suggestions := make([]core.Suggestion, 0, len(groups))
	for _, group := range groups {
		recency := maxFloat(group.recencyScores)
		similarity := maxFloat(group.simScores)
		suggestion := core.Suggestion{
			ID:               suggestionID(group.kind, group.snippet),
			Snippet:          group.snippet,
			Kind:             group.kind,
			Sources:          group.sources,
			Occurrences:      len(group.sources),
			RecencyScore:     recency,
			SystemSimilarity: similarity,
			Confidence:       confidenceForSuggestion(group.kind, len(group.sources), recency, similarity),
		}
		suggestions = append(suggestions, suggestion)
	}

	annotateSuggestionConflicts(suggestions)
	sortSuggestions(suggestions)
	return suggestions
}

func extractCandidates(ranked core.RankedReport) []extractionCandidate {
	report := ranked.Report
	var candidates []extractionCandidate
	add := func(kind, snippet string) {
		snippet = cleanSnippet(snippet)
		if snippet == "" {
			return
		}
		candidates = append(candidates, extractionCandidate{kind: kind, snippet: snippet, ranked: ranked})
	}

	if strings.TrimSpace(report.LaunchOptions) != "" {
		add(core.SuggestionKindLaunchOption, report.LaunchOptions)
	}

	text := strings.TrimSpace(report.Notes + "\n" + report.Verdict)
	for _, snippet := range extractCommandSnippets(text) {
		add(core.SuggestionKindLaunchOption, snippet)
	}
	for _, snippet := range extractEnvAssignments(text) {
		add(core.SuggestionKindEnvVar, snippet)
	}
	for _, snippet := range extractWrapperSnippets(text) {
		add(core.SuggestionKindWrapper, snippet)
	}
	for _, item := range extractWorkaroundSnippets(text) {
		add(item.kind, item.snippet)
	}
	return candidates
}

func extractCommandSnippets(text string) []string {
	var snippets []string
	segments := splitTextSegments(text)
	for _, segment := range segments {
		fields := strings.Fields(segment)
		if len(fields) == 0 {
			continue
		}
		for i, field := range fields {
			if !strings.EqualFold(trimToken(field), "%command%") {
				continue
			}
			start := i
			for j := i - 1; j >= 0; j-- {
				token := trimToken(fields[j])
				if isEnvAssignmentToken(token) || isKnownWrapper(token) || isWrapperOptionToken(token) || isDimensionToken(token) || isNumericToken(token) {
					start = j
					continue
				}
				break
			}
			end := i
			for j := i + 1; j < len(fields); j++ {
				token := trimToken(fields[j])
				if isGameArgToken(token) {
					end = j
					continue
				}
				break
			}
			parts := make([]string, 0, end-start+1)
			for _, token := range fields[start : end+1] {
				cleaned := trimToken(token)
				if cleaned != "" {
					parts = append(parts, cleaned)
				}
			}
			snippet := strings.Join(parts, " ")
			if snippet != "" && !strings.EqualFold(snippet, "%command%") {
				snippets = append(snippets, snippet)
			}
		}
	}
	return dedupeStrings(snippets)
}

func extractEnvAssignments(text string) []string {
	matches := envAssignPattern.FindAllString(text, -1)
	var snippets []string
	for _, match := range matches {
		match = cleanSnippet(match)
		if isKnownEnvAssignment(match) {
			snippets = append(snippets, match)
		}
	}
	return dedupeStrings(snippets)
}

func extractWrapperSnippets(text string) []string {
	fields := strings.Fields(text)
	var snippets []string
	for i := 0; i < len(fields); i++ {
		token := trimToken(fields[i])
		if !isKnownWrapper(token) {
			continue
		}
		end := i
		for j := i + 1; j < len(fields); j++ {
			next := trimToken(fields[j])
			if strings.EqualFold(next, "%command%") {
				break
			}
			if isWrapperOptionToken(next) || isDimensionToken(next) || isNumericToken(next) {
				end = j
				continue
			}
			break
		}
		if end == i && !wrapperMentionHasRunContext(fields, i) {
			continue
		}
		parts := make([]string, 0, end-i+1)
		for _, field := range fields[i : end+1] {
			if cleaned := trimToken(field); cleaned != "" {
				parts = append(parts, cleaned)
			}
		}
		if len(parts) > 0 {
			snippets = append(snippets, strings.Join(parts, " "))
		}
	}
	return dedupeStrings(snippets)
}

type workaroundSnippet struct {
	kind    string
	snippet string
}

func extractWorkaroundSnippets(text string) []workaroundSnippet {
	var snippets []workaroundSnippet
	seen := map[string]bool{}
	for _, item := range knownWorkaroundPatterns {
		for _, match := range item.pattern.FindAllString(text, -1) {
			match = cleanSnippet(match)
			key := item.kind + "\x00" + canonicalSnippet(match)
			if match == "" || seen[key] {
				continue
			}
			seen[key] = true
			snippets = append(snippets, workaroundSnippet{kind: item.kind, snippet: match})
		}
	}
	return snippets
}

func textMentionsConcreteTweak(text string) bool {
	if strings.Contains(strings.ToLower(text), "%command%") {
		return true
	}
	return len(extractEnvAssignments(text)) > 0 || len(extractWrapperSnippets(text)) > 0
}

func splitTextSegments(text string) []string {
	return strings.FieldsFunc(text, func(r rune) bool {
		switch r {
		case '\n', '\r', ';':
			return true
		default:
			return false
		}
	})
}

func cleanSnippet(snippet string) string {
	snippet = strings.TrimSpace(snippet)
	snippet = strings.Trim(snippet, " \t\r\n`.,")
	snippet = trimOuterQuotes(snippet)
	return strings.Join(strings.Fields(snippet), " ")
}

func trimToken(token string) string {
	token = strings.TrimSpace(token)
	token = strings.Trim(token, " \t\r\n`.,:()[]")
	return trimOuterQuotes(token)
}

func trimOuterQuotes(value string) string {
	value = strings.TrimSpace(value)
	for len(value) >= 2 && isQuote(value[0]) && value[len(value)-1] == value[0] {
		value = strings.TrimSpace(value[1 : len(value)-1])
	}
	for len(value) > 0 && isQuote(value[0]) && strings.Count(value, string(value[0]))%2 == 1 {
		value = strings.TrimSpace(value[1:])
	}
	for len(value) > 0 && isQuote(value[len(value)-1]) && strings.Count(value, string(value[len(value)-1]))%2 == 1 {
		value = strings.TrimSpace(value[:len(value)-1])
	}
	return value
}

func isQuote(ch byte) bool {
	return ch == 34 || ch == 39
}

func canonicalSnippet(snippet string) string {
	return strings.ToLower(strings.Join(strings.Fields(cleanSnippet(snippet)), " "))
}

func suggestionID(kind, snippet string) string {
	hash := sha1.Sum([]byte(kind + "\n" + canonicalSnippet(snippet)))
	prefix := map[string]string{
		core.SuggestionKindLaunchOption: "launch",
		core.SuggestionKindEnvVar:       "env",
		core.SuggestionKindWrapper:      "wrap",
		core.SuggestionKindWorkaround:   "work",
		core.SuggestionKindDiagnostic:   "diag",
		core.SuggestionKindNote:         "note",
	}[kind]
	if prefix == "" {
		prefix = "sug"
	}
	return prefix + "-" + hex.EncodeToString(hash[:4])
}

func citationForReport(report core.Report, snippet string) core.Citation {
	reportID := report.SourceReportID
	if reportID == "" && report.ID > 0 {
		reportID = strconv.FormatInt(report.ID, 10)
	}
	timestamp := ""
	if !report.Timestamp.IsZero() {
		timestamp = report.Timestamp.UTC().Format(time.RFC3339)
	}
	return core.Citation{
		SourceID:  report.SourceID,
		ReportID:  reportID,
		AppID:     report.AppID,
		Timestamp: timestamp,
		Snippet:   snippet,
	}
}

func isDangerousSnippet(snippet string) bool {
	lower := strings.ToLower(snippet)
	if strings.ContainsAny(snippet, "\n\r;`<>") || strings.Contains(lower, "$(") || strings.Contains(lower, "&&") || strings.Contains(lower, "||") || strings.Contains(lower, " |") || strings.Contains(lower, "| ") {
		return true
	}
	dangerousWords := []string{"sudo", "pkexec", "rm", "mkfs", "dd", "curl", "wget", "bash", "sh", "python", "perl", "chmod", "chown"}
	fields := strings.Fields(lower)
	for _, field := range fields {
		field = strings.Trim(field, " ./")
		for _, dangerous := range dangerousWords {
			if field == dangerous {
				return true
			}
		}
	}
	return false
}

func isEnvAssignmentToken(token string) bool {
	return isKnownEnvAssignment(token)
}

func isKnownEnvAssignment(token string) bool {
	name, _, ok := splitEnvAssignment(token)
	if !ok {
		return false
	}
	if strings.HasPrefix(name, "PROTON_") || strings.HasPrefix(name, "DXVK_") || strings.HasPrefix(name, "VKD3D_") || strings.HasPrefix(name, "RADV_") || strings.HasPrefix(name, "MESA_") || strings.HasPrefix(name, "WINE") || strings.HasPrefix(name, "__GL_") || strings.HasPrefix(name, "NVIDIA_") || strings.HasPrefix(name, "AMD_") || strings.HasPrefix(name, "SDL_") {
		return true
	}
	switch name {
	case "WINEDLLOVERRIDES", "MANGOHUD", "ENABLE_VKBASALT", "PULSE_LATENCY_MSEC":
		return true
	default:
		return false
	}
}

func splitEnvAssignment(token string) (name, value string, ok bool) {
	token = cleanSnippet(token)
	idx := strings.Index(token, "=")
	if idx <= 0 || idx == len(token)-1 {
		return "", "", false
	}
	name = strings.ToUpper(strings.TrimSpace(token[:idx]))
	value = strings.Trim(strings.TrimSpace(token[idx+1:]), "\"'")
	if name == "" || value == "" {
		return "", "", false
	}
	return name, value, true
}

func isKnownWrapper(token string) bool {
	switch strings.ToLower(token) {
	case "gamemoderun", "mangohud", "gamescope", "prime-run", "obs-gamecapture":
		return true
	default:
		return false
	}
}

func isWrapperOptionToken(token string) bool {
	if token == "--" {
		return true
	}
	return strings.HasPrefix(token, "-") && len(token) > 1 && !strings.ContainsAny(token, ";|&`<>")
}

func isGameArgToken(token string) bool {
	if token == "" || strings.ContainsAny(token, ";|&`<>") {
		return false
	}
	return strings.HasPrefix(token, "-") || strings.HasPrefix(token, "+") || strings.Contains(token, "=")
}

func isDimensionToken(token string) bool {
	matched, _ := regexp.MatchString(`^\d+x\d+$`, strings.ToLower(token))
	return matched
}

func isNumericToken(token string) bool {
	if token == "" {
		return false
	}
	_, err := strconv.Atoi(token)
	return err == nil
}

func wrapperMentionHasRunContext(fields []string, index int) bool {
	start := index - 3
	if start < 0 {
		start = 0
	}
	for _, field := range fields[start:index] {
		switch strings.ToLower(trimToken(field)) {
		case "use", "using", "run", "try", "with", "via", "enable", "enabled":
			return true
		}
	}
	if index+1 < len(fields) && strings.EqualFold(trimToken(fields[index+1]), "%command%") {
		return true
	}
	return false
}

func confidenceForSuggestion(kind string, occurrences int, recency, similarity float64) string {
	concrete := kind == core.SuggestionKindLaunchOption || kind == core.SuggestionKindEnvVar || kind == core.SuggestionKindWrapper
	if concrete && occurrences >= 2 && recency >= 0.75 && similarity >= 0.60 {
		return core.SuggestionConfidenceHigh
	}
	if concrete && (recency >= 0.40 || occurrences >= 2) {
		return core.SuggestionConfidenceMedium
	}
	if kind == core.SuggestionKindWorkaround && occurrences >= 2 && recency >= 0.75 {
		return core.SuggestionConfidenceMedium
	}
	return core.SuggestionConfidenceLow
}

func annotateSuggestionConflicts(suggestions []core.Suggestion) {
	type valueRefs map[string][]int
	byName := map[string]valueRefs{}
	for i, suggestion := range suggestions {
		for name, value := range envAssignmentsInSnippet(suggestion.Snippet) {
			if byName[name] == nil {
				byName[name] = valueRefs{}
			}
			byName[name][value] = append(byName[name][value], i)
		}
	}
	for name, values := range byName {
		if len(values) < 2 {
			continue
		}
		valueNames := make([]string, 0, len(values))
		affected := map[int]bool{}
		for value, refs := range values {
			valueNames = append(valueNames, value)
			for _, ref := range refs {
				affected[ref] = true
			}
		}
		sort.Strings(valueNames)
		note := fmt.Sprintf("Conflicts with other suggestions setting %s to %s", name, strings.Join(valueNames, ", "))
		for index := range affected {
			suggestions[index].ConflictNotes = appendUnique(suggestions[index].ConflictNotes, note)
		}
	}
}

func envAssignmentsInSnippet(snippet string) map[string]string {
	result := map[string]string{}
	for _, match := range envAssignPattern.FindAllString(snippet, -1) {
		name, value, ok := splitEnvAssignment(match)
		if ok {
			result[name] = value
		}
	}
	return result
}

func sortSuggestions(suggestions []core.Suggestion) {
	sort.SliceStable(suggestions, func(i, j int) bool {
		if confidenceRank(suggestions[i].Confidence) != confidenceRank(suggestions[j].Confidence) {
			return confidenceRank(suggestions[i].Confidence) > confidenceRank(suggestions[j].Confidence)
		}
		if suggestions[i].Occurrences != suggestions[j].Occurrences {
			return suggestions[i].Occurrences > suggestions[j].Occurrences
		}
		if suggestions[i].RecencyScore != suggestions[j].RecencyScore {
			return suggestions[i].RecencyScore > suggestions[j].RecencyScore
		}
		if suggestions[i].SystemSimilarity != suggestions[j].SystemSimilarity {
			return suggestions[i].SystemSimilarity > suggestions[j].SystemSimilarity
		}
		if kindRank(suggestions[i].Kind) != kindRank(suggestions[j].Kind) {
			return kindRank(suggestions[i].Kind) < kindRank(suggestions[j].Kind)
		}
		return suggestions[i].Snippet < suggestions[j].Snippet
	})
}

func confidenceRank(confidence string) int {
	switch confidence {
	case core.SuggestionConfidenceHigh:
		return 3
	case core.SuggestionConfidenceMedium:
		return 2
	default:
		return 1
	}
}

func kindRank(kind string) int {
	switch kind {
	case core.SuggestionKindLaunchOption:
		return 0
	case core.SuggestionKindEnvVar:
		return 1
	case core.SuggestionKindWrapper:
		return 2
	case core.SuggestionKindWorkaround:
		return 3
	case core.SuggestionKindDiagnostic:
		return 4
	default:
		return 5
	}
}

func maxFloat(values []float64) float64 {
	max := 0.0
	for _, value := range values {
		if value > max {
			max = value
		}
	}
	return roundScore(max)
}

func dedupeStrings(values []string) []string {
	seen := map[string]bool{}
	var out []string
	for _, value := range values {
		value = cleanSnippet(value)
		key := canonicalSnippet(value)
		if value == "" || seen[key] {
			continue
		}
		seen[key] = true
		out = append(out, value)
	}
	return out
}

func appendUnique(values []string, next string) []string {
	for _, value := range values {
		if value == next {
			return values
		}
	}
	return append(values, next)
}
