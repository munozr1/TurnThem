#include "tt/bot.h"
#include <vector>

namespace tt {

std::optional<BotChoice> ChooseBotPlacement(const Grid& grid, Side side, std::mt19937_64& rng,
                                            const std::function<bool(int, int)>& allowed) {
    // how far toward the enemy a row is, from this side's point of view
    auto forward = [side](int row) { return side == Side::White ? kRows - 1 - row : row; };

    std::vector<BotChoice> candidates;
    int best = -1;
    for (int row = 0; row < kRows; row++) {
        for (int col = 0; col < kCols; col++) {
            if (ValidatePlacement(grid, side, col, row) != PlaceError::Ok) continue;
            if (allowed && !allowed(col, row)) continue;
            candidates.push_back({(uint8_t)col, (uint8_t)row});
            if (forward(row) > best) best = forward(row);
        }
    }
    // keep the cells within two rows of the front, then pick one at random
    std::vector<BotChoice> front;
    for (const BotChoice& c : candidates)
        if (forward(c.row) >= best - 2) front.push_back(c);
    if (front.empty()) return std::nullopt;
    return front[rng() % front.size()];
}

} // namespace tt
