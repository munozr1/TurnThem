#pragma once
#include "tt/weapons.h"

// Presentation follows the shot, not the simulation's SCannon windup frames.
// No wall clock: playback, mirrored sides and delayed clients use the same pose.
struct WeaponVisual {
    int frame = 0;
    int shot_ticks = -1;
};

inline WeaponVisual WeaponVisualFor(const tt::WeaponState& weapon) {
    WeaponVisual visual;
    if (weapon.type == tt::WeaponType::Cannon && weapon.age > 0) {
        visual.shot_ticks = (int)((weapon.age - 1) % tt::kCannonFireInterval);
    } else if (weapon.type == tt::WeaponType::SCannon &&
               weapon.phase == tt::WeaponState::Phase::ShotDelay) {
        visual.shot_ticks = weapon.timer;
    }
    if (visual.shot_ticks >= 0 && visual.shot_ticks < 4) visual.frame = 1;
    else if (visual.shot_ticks >= 4 && visual.shot_ticks < 12) visual.frame = 2;
    return visual;
}
