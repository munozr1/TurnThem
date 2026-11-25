#pragma once
#include <functional>
#include <optional>
#include <random>
#include "tt/grid.h"

namespace tt {

struct BotChoice {
    uint8_t col, row;
};

// A legal deployment near the fixed front line. When supplied, `allowed` is
// the card-specific validator (infantry need no artillery clearance).
// Used for practice-mode opponents and the headless test bot.
std::optional<BotChoice> ChooseBotPlacement(const Grid& grid, Side side, std::mt19937_64& rng,
                                            const std::function<bool(int, int)>& allowed = {});

} // namespace tt
