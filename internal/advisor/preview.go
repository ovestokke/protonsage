package advisor

import (
	"fmt"
	"sort"
	"strings"

	"protonsage/internal/core"
)

// BuildLaunchPreview composes selected copy/export suggestions into one Steam launch-option string.
// It never writes Steam config.
func BuildLaunchPreview(selected []core.Suggestion, existing string) core.PreviewResult {
	result := core.PreviewResult{Existing: strings.TrimSpace(existing)}
	var prefixTokens []string
	var suffixTokens []string

	if result.Existing != "" {
		prefix, suffix, commandCount := splitLaunchTokens(result.Existing)
		switch commandCount {
		case 0:
			tokens := cleanedFields(result.Existing)
			if tokensLookLikePrefix(tokens) {
				prefixTokens = append(prefixTokens, tokens...)
				result.Warnings = append(result.Warnings, "Existing launch options had no %command%; preview inserts %command% after existing env/wrapper tokens.")
			} else {
				suffixTokens = append(suffixTokens, tokens...)
				result.Warnings = append(result.Warnings, "Existing launch options had no %command%; preview preserves them after %command% as game arguments.")
			}
		case 1:
			prefixTokens = append(prefixTokens, prefix...)
			suffixTokens = append(suffixTokens, suffix...)
		default:
			prefixTokens = append(prefixTokens, prefix...)
			suffixTokens = append(suffixTokens, suffix...)
			result.Warnings = append(result.Warnings, "Existing launch options contained multiple %command% tokens; preview keeps one.")
		}
	}

	for _, suggestion := range selected {
		if !isCopyableSuggestion(suggestion) {
			result.Skipped = append(result.Skipped, suggestion)
			result.Warnings = append(result.Warnings, fmt.Sprintf("Skipped %s suggestion %s because notes/workarounds are not automatic launch options.", suggestion.Kind, suggestion.ID))
			continue
		}
		if isDangerousSnippet(suggestion.Snippet) {
			result.Skipped = append(result.Skipped, suggestion)
			result.Warnings = append(result.Warnings, fmt.Sprintf("Skipped suggestion %s because it contains shell control or destructive command tokens.", suggestion.ID))
			continue
		}

		prefix, suffix, commandCount := splitLaunchTokens(suggestion.Snippet)
		if commandCount > 0 {
			prefixTokens = append(prefixTokens, prefix...)
			suffixTokens = append(suffixTokens, suffix...)
		} else {
			tokens := cleanedFields(suggestion.Snippet)
			if suggestion.Kind == core.SuggestionKindEnvVar || suggestion.Kind == core.SuggestionKindWrapper || tokensLookLikePrefix(tokens) {
				prefixTokens = append(prefixTokens, tokens...)
			} else {
				suffixTokens = append(suffixTokens, tokens...)
			}
		}
		result.Applied = append(result.Applied, suggestion)
		for _, note := range suggestion.ConflictNotes {
			result.Conflicts = appendUnique(result.Conflicts, note)
		}
	}

	prefixTokens = orderPrefixTokens(dedupeTokens(prefixTokens))
	suffixTokens = dedupeTokens(suffixTokens)
	for _, conflict := range envConflictsInTokens(prefixTokens) {
		result.Conflicts = appendUnique(result.Conflicts, conflict)
	}
	if len(result.Conflicts) > 0 {
		result.Warnings = append(result.Warnings, "Preview includes conflicting environment assignments; choose one before copying.")
	}

	parts := make([]string, 0, len(prefixTokens)+1+len(suffixTokens))
	parts = append(parts, prefixTokens...)
	parts = append(parts, "%command%")
	parts = append(parts, suffixTokens...)
	result.Preview = strings.Join(parts, " ")
	result.Warnings = dedupeOrdered(result.Warnings)
	result.Conflicts = dedupeOrdered(result.Conflicts)
	return result
}

func isCopyableSuggestion(suggestion core.Suggestion) bool {
	switch suggestion.Kind {
	case core.SuggestionKindLaunchOption, core.SuggestionKindEnvVar, core.SuggestionKindWrapper:
		return true
	default:
		return false
	}
}

func splitLaunchTokens(value string) (prefix []string, suffix []string, commandCount int) {
	tokens := cleanedFields(value)
	firstCommand := -1
	for i, token := range tokens {
		if strings.EqualFold(token, "%command%") {
			commandCount++
			if firstCommand < 0 {
				firstCommand = i
			}
		}
	}
	if firstCommand < 0 {
		return nil, nil, 0
	}
	for _, token := range tokens[:firstCommand] {
		if !strings.EqualFold(token, "%command%") {
			prefix = append(prefix, token)
		}
	}
	for _, token := range tokens[firstCommand+1:] {
		if !strings.EqualFold(token, "%command%") {
			suffix = append(suffix, token)
		}
	}
	return prefix, suffix, commandCount
}

func cleanedFields(value string) []string {
	fields := strings.Fields(value)
	out := make([]string, 0, len(fields))
	for _, field := range fields {
		cleaned := trimToken(field)
		if cleaned != "" {
			out = append(out, cleaned)
		}
	}
	return out
}

func tokensLookLikePrefix(tokens []string) bool {
	if len(tokens) == 0 {
		return false
	}
	if !isEnvAssignmentToken(tokens[0]) && !isKnownWrapper(tokens[0]) {
		return false
	}
	for _, token := range tokens[1:] {
		if isEnvAssignmentToken(token) || isKnownWrapper(token) || isWrapperOptionToken(token) || isDimensionToken(token) || isNumericToken(token) {
			continue
		}
		return false
	}
	return true
}

func orderPrefixTokens(tokens []string) []string {
	ordered := make([]string, 0, len(tokens))
	for _, token := range tokens {
		if isEnvAssignmentToken(token) {
			ordered = append(ordered, token)
		}
	}
	for _, token := range tokens {
		if !isEnvAssignmentToken(token) {
			ordered = append(ordered, token)
		}
	}
	return ordered
}

func dedupeTokens(tokens []string) []string {
	seen := map[string]bool{}
	out := make([]string, 0, len(tokens))
	for _, token := range tokens {
		key := canonicalSnippet(token)
		if key == "" || seen[key] {
			continue
		}
		seen[key] = true
		out = append(out, token)
	}
	return out
}

func envConflictsInTokens(tokens []string) []string {
	valuesByName := map[string]map[string]bool{}
	for _, token := range tokens {
		name, value, ok := splitEnvAssignment(token)
		if !ok {
			continue
		}
		if valuesByName[name] == nil {
			valuesByName[name] = map[string]bool{}
		}
		valuesByName[name][value] = true
	}
	var conflicts []string
	for name, values := range valuesByName {
		if len(values) < 2 {
			continue
		}
		valueList := make([]string, 0, len(values))
		for value := range values {
			valueList = append(valueList, value)
		}
		sort.Strings(valueList)
		conflicts = append(conflicts, fmt.Sprintf("Conflicting %s values selected: %s", name, strings.Join(valueList, ", ")))
	}
	sort.Strings(conflicts)
	return conflicts
}

func dedupeOrdered(values []string) []string {
	seen := map[string]bool{}
	out := make([]string, 0, len(values))
	for _, value := range values {
		if value == "" || seen[value] {
			continue
		}
		seen[value] = true
		out = append(out, value)
	}
	return out
}
