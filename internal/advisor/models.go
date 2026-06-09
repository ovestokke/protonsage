package advisor

import "protonsage/internal/core"

// DraftRecommendation returns a placeholder deterministic recommendation shape.
// Real scoring/extraction will be added after data import and report retrieval exist.
func DraftRecommendation(game core.Game) core.Recommendation {
	return core.Recommendation{
		Game:        game,
		Summary:     "No ProtonDB reports imported yet. Import data to generate cited recommendations.",
		Suggestions: nil,
		GeneratedBy: core.RecommendationSourceRules,
	}
}
