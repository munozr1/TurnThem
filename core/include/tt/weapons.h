#pragma once
#include "tt/types.h"

namespace tt {

// Gameplay constants, in ticks at kTickRate. These reproduce the original
// Cannon/SCannon timing (which mixed GetTime() and per-frame counters) at 60 fps.
constexpr int kAnimFrames = 3;
constexpr int kAnimTicksPerFrame = 10;
constexpr int kCannonFireInterval = 30;      // 0.5 s
constexpr float kCannonSpeed = 800.0f;       // px/s
constexpr float kSCannonSpeed = 500.0f;      // px/s
constexpr float kSCannonBarrel = 18.0f;      // px from the cell center to the muzzle
constexpr int kSCannonSwivelStep = 2;        // degrees per tick
constexpr int kSCannonShotDelay = 18;        // 0.3 s between shots
constexpr int kSCannonIdle = 60;             // 1 s between sweeps
constexpr int kSCannonPatternLen = 5;
constexpr int kSCannonPattern[kSCannonPatternLen] = {0, 45, 0, -45, 0}; // center, right, center, left, center

struct WeaponState {
    enum class Phase : uint8_t { Idle, Rotating, Animating, ShotDelay };

    uint32_t id = 0;
    WeaponType type = WeaponType::Cannon;
    Side side = Side::White;
    uint8_t col = 0, row = 0;
    uint32_t placed_tick = 0;
    uint32_t age = 0;           // ticks stepped so far
    // SCannon sweep state (a Cannon only uses age)
    Phase phase = Phase::Rotating;
    int angle_deg = 0;          // barrel angle in the owner's frame, 0 = straight at the enemy
    uint8_t pattern_idx = 0;
    int timer = 0;              // ticks spent in the current phase / animation frame
    uint8_t anim_frame = 0;     // which animation frame to draw
};

struct Shot {
    Vec2 pos;
    Vec2 vel; // px/s
};

WeaponState MakeWeapon(uint32_t id, WeaponType type, Side side, int col, int row, uint32_t tick);

// Advances one tick. Returns true when the weapon fires, filling *shot if given.
// Black's weapons are the point reflection of white's (the board is symmetric
// under (col,row) -> (12-col, 19-row)), so their shots are negated.
bool StepWeapon(WeaponState& weapon, Shot* shot);

// world-space sprite rotation in degrees
float SpriteRotationDeg(const WeaponState& weapon);

// unit vector for an angle measured like raylib (0 = +x, 90 = +y); exact for multiples of 45
Vec2 DirFromDeg(int deg);

} // namespace tt
