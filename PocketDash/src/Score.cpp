#include "Score.h"

#include "LevelSession.h"

#include <algorithm>
#include <cmath>

namespace pd {

int ScoreBreakdown::total() const {
    return static_cast<int>(std::lround(static_cast<float>(subtotal()) * multiplier));
}

ScoreBreakdown computeScore(const LevelSession& session) {
    ScoreBreakdown s;
    const SessionStats& stats = session.stats();
    s.coins = stats.coinPoints * ScoreBreakdown::kCoin;
    s.foes = stats.enemiesDefeated * ScoreBreakdown::kFoe;
    s.stars = session.items().starsCollected() * ScoreBreakdown::kStar;
    s.secrets = session.secretsFound() * ScoreBreakdown::kSecret;
    s.hearts = std::max(0, session.hearts()) * ScoreBreakdown::kHeart;
    // Par is the level's own time limit (without the Relaxed extension), so
    // easier settings don't earn a bigger time bonus.
    const float par = session.level().timeLimit > 0.0f ? session.level().timeLimit : ScoreBreakdown::kDefaultPar;
    s.time = static_cast<int>(std::max(0.0f, par - session.time())) * ScoreBreakdown::kPerSecond;
    s.golden = session.goldenStar() ? ScoreBreakdown::kGolden : 0;
    s.multiplier = session.rules().scoreMultiplier;
    return s;
}

} // namespace pd
