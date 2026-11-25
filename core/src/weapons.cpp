#include "tt/weapons.h"
#include "tt/grid.h"
#include <math.h>
#include <stdlib.h>

namespace tt {

WeaponState MakeWeapon(uint32_t id, WeaponType type, Side side, int col, int row, uint32_t tick) {
    WeaponState weapon;
    weapon.id = id;
    weapon.type = type;
    weapon.side = side;
    weapon.col = (uint8_t)col;
    weapon.row = (uint8_t)row;
    weapon.placed_tick = tick;
    return weapon; // a SCannon starts its first sweep immediately, like the original
}

Vec2 DirFromDeg(int deg) {
    deg %= 360;
    if (deg < 0) deg += 360;
    const float h = 0.70710678f;
    switch (deg) {
        case 0: return {1, 0};
        case 45: return {h, h};
        case 90: return {0, 1};
        case 135: return {-h, h};
        case 180: return {-1, 0};
        case 225: return {-h, -h};
        case 270: return {0, -1};
        case 315: return {h, -h};
        default: break;
    }
    const float rad = deg * 3.14159265358979f / 180.0f;
    return {cosf(rad), sinf(rad)};
}

// owner-frame offset/velocity to world: black is rotated 180 degrees
static Vec2 ToWorld(Side side, Vec2 v) {
    return side == Side::White ? v : Vec2{-v.x, -v.y};
}

static bool StepCannon(WeaponState& w, Shot* shot) {
    const int m = (int)(w.age % kCannonFireInterval);
    // frame advances every kAnimTicksPerFrame ticks after a shot, then rests on frame 0
    w.anim_frame = (uint8_t)(((m + 1) / kAnimTicksPerFrame) % kAnimFrames);
    const bool fired = m == 0;
    if (fired && shot) {
        const Vec2 dir = DirFromDeg(-90); // owner frame: straight up the board
        shot->pos = CellCenter(w.col, w.row);
        shot->vel = ToWorld(w.side, {dir.x * kCannonSpeed, dir.y * kCannonSpeed});
    }
    w.age++;
    return fired;
}

static void SwivelStep(WeaponState& w) {
    const int target = kSCannonPattern[w.pattern_idx];
    const int diff = target - w.angle_deg;
    if (abs(diff) > 1) {
        w.angle_deg += diff > 0 ? kSCannonSwivelStep : -kSCannonSwivelStep;
    } else {
        w.angle_deg = target;
        w.phase = WeaponState::Phase::Animating;
        w.timer = 0;
        w.anim_frame = 0;
    }
}

static bool StepSCannon(WeaponState& w, Shot* shot) {
    using Phase = WeaponState::Phase;
    switch (w.phase) {
        case Phase::Idle:
            if (++w.timer >= kSCannonIdle) {
                w.pattern_idx = 0;
                w.timer = 0;
                w.phase = Phase::Rotating;
                SwivelStep(w);
            }
            break;
        case Phase::ShotDelay:
            if (++w.timer >= kSCannonShotDelay) {
                w.pattern_idx = (uint8_t)((w.pattern_idx + 1) % kSCannonPatternLen);
                w.timer = 0;
                if (w.pattern_idx == 0) {
                    w.phase = Phase::Idle;
                } else {
                    w.phase = Phase::Rotating;
                    SwivelStep(w);
                }
            }
            break;
        case Phase::Rotating:
            SwivelStep(w);
            break;
        case Phase::Animating:
            break;
    }

    // the animation runs in the same tick the barrel arrives, and fires on its last frame
    bool fired = false;
    if (w.phase == Phase::Animating && ++w.timer >= kAnimTicksPerFrame) {
        w.timer = 0;
        if (++w.anim_frame >= kAnimFrames) {
            fired = true;
            if (shot) {
                const Vec2 dir = DirFromDeg(w.angle_deg - 90);
                const Vec2 center = CellCenter(w.col, w.row);
                const Vec2 offset = ToWorld(w.side, {dir.x * kSCannonBarrel, dir.y * kSCannonBarrel});
                shot->pos = {center.x + offset.x, center.y + offset.y};
                shot->vel = ToWorld(w.side, {dir.x * kSCannonSpeed, dir.y * kSCannonSpeed});
            }
            w.anim_frame = 0;
            w.phase = Phase::ShotDelay;
        }
    }
    w.age++;
    return fired;
}

bool StepWeapon(WeaponState& weapon, Shot* shot) {
    switch (weapon.type) {
        case WeaponType::Cannon: return StepCannon(weapon, shot);
        case WeaponType::SCannon: return StepSCannon(weapon, shot);
        case WeaponType::Count: break;
    }
    weapon.age++;
    return false;
}

float SpriteRotationDeg(const WeaponState& weapon) {
    const float angle = weapon.type == WeaponType::SCannon ? (float)weapon.angle_deg : 0.0f;
    return weapon.side == Side::White ? angle : angle + 180.0f;
}

} // namespace tt
