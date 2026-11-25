#pragma once
#include <functional>
#include <optional>
#include <random>
#include "tt/grid.h"

namespace tt {

struct BotChoice {
    uint8_t col, row;
};

// A legal own-colour cell near the front line, or nothing. `allowed` can veto
// cells the board alone doesn't rule out (e.g. a client's pending placements).
// Used for practice-mode opponents and the headless test bot.
std::optional<BotChoice> ChooseBotPlacement(const Grid& grid, Side side, std::mt19937_64& rng,
                                            const std::function<bool(int, int)>& allowed = {});

} // namespace tt
