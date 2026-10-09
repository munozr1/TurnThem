#pragma once
#include <algorithm>
#include <cstdint>
#include "tt/types.h"

// Elixir pays for cards. It's counted in points: one point per tick at normal
// speed, so 1 elixir = kElixirTicks points = 2.8 s. Integers only, so the
// client's mirror of its own elixir matches the server's exactly.
namespace tt {

using ElixirPoints = int32_t;

constexpr int kElixirTicks = 168;                   // 2.8 s per elixir at normal speed
constexpr int kMaxElixir = 10;
constexpr int kStartElixir = 5;
constexpr uint32_t kDoubleElixirTicks = 60 * kTickRate; // the last minute regenerates twice as fast

constexpr ElixirPoints ElixirToPoints(int elixir) { return elixir * kElixirTicks; }
constexpr ElixirPoints kMaxElixirPoints = ElixirToPoints(kMaxElixir);
constexpr ElixirPoints kStartElixirPoints = ElixirToPoints(kStartElixir);

constexpr int CardCost(WeaponType) { return 3; }
constexpr ElixirPoints CardCostPoints(WeaponType type) { return ElixirToPoints(CardCost(type)); }

// points gained on `tick`: nothing outside play (countdown, after the end),
// double in the last kDoubleElixirTicks
constexpr ElixirPoints RegenOnTick(uint32_t tick, uint32_t start_tick, uint32_t end_tick) {
    if (tick < start_tick || tick >= end_tick) return 0;
    return tick + kDoubleElixirTicks >= end_tick ? 2 : 1;
}

constexpr ElixirPoints Regenerate(ElixirPoints points, uint32_t tick, uint32_t start_tick, uint32_t end_tick) {
    return std::min(kMaxElixirPoints, points + RegenOnTick(tick, start_tick, end_tick));
}

} // namespace tt
