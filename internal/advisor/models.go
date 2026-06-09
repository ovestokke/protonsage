package advisor

import (
	"time"

	"protonsage/internal/core"
)

// DraftRecommendation returns the deterministic empty-report recommendation shape.
func DraftRecommendation(game core.Game) core.Recommendation {
	return GenerateRecommendation(game, nil, core.SystemProfile{}, time.Time{})
}
