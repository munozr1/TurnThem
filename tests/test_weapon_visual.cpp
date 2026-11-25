#include "doctest/doctest.h"
#include "weapon_visual.h"

TEST_CASE("weapon art recoils only after real shots for either side") {
    for (tt::WeaponType type : {tt::WeaponType::Cannon, tt::WeaponType::SCannon}) {
        for (tt::Side side : {tt::Side::White, tt::Side::Black}) {
            auto weapon = tt::MakeWeapon(1, type, side, 6, 12, 0);
            CHECK(WeaponVisualFor(weapon).frame == 0);
            CHECK(WeaponVisualFor(weapon).shot_ticks == -1);
            int since_shot = 1000;
            for (int tick = 0; tick < 900; tick++) {
                const bool fired = tt::StepWeapon(weapon, nullptr);
                since_shot = fired ? 0 : since_shot + 1;
                const WeaponVisual visual = WeaponVisualFor(weapon);
                CAPTURE((int)type);
                CAPTURE((int)side);
                CAPTURE(tick);
                CAPTURE(since_shot);
                CHECK(visual.frame == (since_shot < 4 ? 1 : since_shot < 12 ? 2 : 0));
                CHECK((visual.shot_ticks >= 0 && visual.shot_ticks < 3) == (since_shot < 3));
                if (fired) CHECK(visual.shot_ticks == 0);
            }
        }
    }
}
