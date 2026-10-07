#pragma once
#include "tt/types.h"

// The game is laid out in this fixed coordinate space (the board on top, a HUD
// strip, then the deck) and scaled to fit the window, so it also fits screens
// shorter than 990 px.
constexpr int kScreenW = tt::kBoardW;          // 520
constexpr int kHudY = tt::kBoardH;             // 800
constexpr int kHudH = 40;
constexpr int kDeckY = kHudY + kHudH;          // 840
constexpr int kDeckH = 150;
constexpr int kScreenH = kDeckY + kDeckH;      // 990
