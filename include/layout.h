#pragma once
#include "tt/types.h"

// The game is laid out in this fixed coordinate space (the board on top, a HUD
// strip, the deck, the elixir bar) and scaled to fit the window, so it also fits screens
// shorter than the layout.
constexpr int kScreenW = tt::kBoardW;          // 520
constexpr int kHudY = tt::kBoardH;             // 800
constexpr int kHudH = 40;
constexpr int kDeckY = kHudY + kHudH;          // 840
constexpr int kDeckH = 150;
constexpr int kElixirY = kDeckY + kDeckH;      // 990
constexpr int kElixirH = 28;
constexpr int kScreenH = kElixirY + kElixirH;  // 1018
