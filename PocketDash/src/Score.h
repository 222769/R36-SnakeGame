#pragma once

namespace pd {

class LevelSession;

// Points awarded at the end of a level. Every part is shown on the results
// panel, so the rules are easy to read:
//   coins      10 each (Double Coins counts them twice)
//   foes       50 each, 1500 for calming a boss
//   stars     500 each
//   secrets   300 each
//   hearts    200 per heart left
//   time       10 per second under par (the time limit, or 2 minutes)
//   golden  2000 for finding everything in one run
// The sum is multiplied by the difficulty's score multiplier.
struct ScoreBreakdown {
    static constexpr int kCoin = 10;
    static constexpr int kFoe = 50;
    static constexpr int kBoss = 1500;
    static constexpr int kStar = 500;
    static constexpr int kSecret = 300;
    static constexpr int kHeart = 200;
    static constexpr int kPerSecond = 10;
    static constexpr int kGolden = 2000;
    static constexpr float kDefaultPar = 120.0f;

    int coins = 0;
    int foes = 0;
    int stars = 0;
    int secrets = 0;
    int hearts = 0;
    int time = 0;
    int golden = 0;
    float multiplier = 1.0f;

    int subtotal() const { return coins + foes + stars + secrets + hearts + time + golden; }
    int total() const;
};

ScoreBreakdown computeScore(const LevelSession& session);

} // namespace pd
