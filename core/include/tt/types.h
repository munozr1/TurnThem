#pragma once
#include <cstdint>

// Shared by the server, the client and the tests. No raylib here: the server
// builds on headless machines, and raylib's WHITE/BLACK/... macros would clash.
namespace tt {

constexpr int kCols = 13;
constexpr int kRows = 20;
constexpr int kCell = 40;                  // px
constexpr int kBoardW = kCols * kCell;     // 520
constexpr int kBoardH = kRows * kCell;     // 800
constexpr int kTickRate = 60;              // simulation ticks per second
constexpr float kDt = 1.0f / kTickRate;
constexpr int kHandSize = 4;
constexpr float kOutOfBoundsMargin = 50.0f; // projectiles live this far past the board

struct Vec2 {
    float x, y;
};

enum class Side : uint8_t { White = 0, Black = 1 };
inline Side Opponent(Side s) { return s == Side::White ? Side::Black : Side::White; }

enum class WeaponType : uint8_t { Cannon = 0, SCannon = 1, Count };

} // namespace tt
