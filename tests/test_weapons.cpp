#include <vector>
#include "doctest/doctest.h"
#include "tt/grid.h"
#include "tt/weapons.h"

using namespace tt;

struct Fired {
    uint32_t age;
    int angle;
    Shot shot;
};

static std::vector<Fired> Run(WeaponState weapon, uint32_t ticks) {
    std::vector<Fired> fired;
    for (uint32_t i = 0; i < ticks; i++) {
        const uint32_t age = weapon.age;
        Shot shot;
        if (StepWeapon(weapon, &shot)) fired.push_back({age, weapon.angle_deg, shot});
    }
    return fired;
}

TEST_CASE("cannon fires on placement and then every 0.5 s, straight at the enemy") {
    const auto fired = Run(MakeWeapon(1, WeaponType::Cannon, Side::White, 6, 12, 0), 100);
    REQUIRE(fired.size() == 4);
    CHECK(fired[0].age == 0);
    CHECK(fired[1].age == 30);
    CHECK(fired[2].age == 60);
    CHECK(fired[3].age == 90);
    CHECK(fired[0].shot.pos.x == 260);
    CHECK(fired[0].shot.pos.y == 500);
    CHECK(fired[0].shot.vel.x == 0);
    CHECK(fired[0].shot.vel.y == -kCannonSpeed);

    const auto black = Run(MakeWeapon(2, WeaponType::Cannon, Side::Black, 6, 7, 0), 1);
    REQUIRE(black.size() == 1);
    CHECK(black[0].shot.vel.y == kCannonSpeed);
}

TEST_CASE("cannon animation cycles through its three frames after each shot") {
    WeaponState weapon = MakeWeapon(1, WeaponType::Cannon, Side::White, 6, 12, 0);
    std::vector<int> frames;
    for (int i = 0; i < 31; i++) {
        StepWeapon(weapon, nullptr);
        frames.push_back(weapon.anim_frame);
    }
    CHECK(frames[0] == 0);
    CHECK(frames[8] == 0);
    CHECK(frames[9] == 1);
    CHECK(frames[18] == 1);
    CHECK(frames[19] == 2);
    CHECK(frames[28] == 2);
    CHECK(frames[29] == 0);
    CHECK(frames[30] == 0);
}

TEST_CASE("scannon sweep matches the original timeline") {
    const auto fired = Run(MakeWeapon(1, WeaponType::SCannon, Side::White, 6, 12, 0), 900);
    const uint32_t ages[] = {29, 98, 167, 236, 305, 412, 481, 550, 619, 688, 795, 864};
    const int angles[] = {0, 45, 0, -45, 0, 0, 45, 0, -45, 0, 0, 45};
    REQUIRE(fired.size() == 12);
    for (size_t i = 0; i < fired.size(); i++) {
        CAPTURE(i);
        CHECK(fired[i].age == ages[i]);
        CHECK(fired[i].angle == angles[i]);
    }
}

TEST_CASE("scannon shots leave from the barrel tip, 18 px out") {
    const auto fired = Run(MakeWeapon(1, WeaponType::SCannon, Side::White, 6, 12, 0), 100);
    REQUIRE(fired.size() == 2);
    const Vec2 c = CellCenter(6, 12);
    CHECK(fired[0].shot.pos.x == doctest::Approx(c.x));
    CHECK(fired[0].shot.pos.y == doctest::Approx(c.y - kSCannonBarrel));
    CHECK(fired[0].shot.vel.y == doctest::Approx(-kSCannonSpeed));
    const float h = 0.70710678f;
    CHECK(fired[1].shot.pos.x == doctest::Approx(c.x + kSCannonBarrel * h));
    CHECK(fired[1].shot.pos.y == doctest::Approx(c.y - kSCannonBarrel * h));
    CHECK(fired[1].shot.vel.x == doctest::Approx(kSCannonSpeed * h));
    CHECK(fired[1].shot.vel.y == doctest::Approx(-kSCannonSpeed * h));
}

TEST_CASE("black weapons are the point reflection of white ones") {
    for (WeaponType type : {WeaponType::Cannon, WeaponType::SCannon}) {
        CAPTURE((int)type);
        const auto white = Run(MakeWeapon(1, type, Side::White, 4, 13, 0), 500);
        const auto black = Run(MakeWeapon(2, type, Side::Black, kCols - 1 - 4, kRows - 1 - 13, 0), 500);
        REQUIRE(white.size() == black.size());
        REQUIRE(!white.empty());
        for (size_t i = 0; i < white.size(); i++) {
            CHECK(black[i].age == white[i].age);
            CHECK(black[i].shot.pos.x == doctest::Approx(kBoardW - white[i].shot.pos.x));
            CHECK(black[i].shot.pos.y == doctest::Approx(kBoardH - white[i].shot.pos.y));
            CHECK(black[i].shot.vel.x == doctest::Approx(-white[i].shot.vel.x));
            CHECK(black[i].shot.vel.y == doctest::Approx(-white[i].shot.vel.y));
        }
    }
}

TEST_CASE("sprite rotation adds 180 degrees for black") {
    WeaponState weapon = MakeWeapon(1, WeaponType::SCannon, Side::White, 6, 12, 0);
    weapon.angle_deg = 45;
    CHECK(SpriteRotationDeg(weapon) == 45);
    weapon.side = Side::Black;
    CHECK(SpriteRotationDeg(weapon) == 225);
    CHECK(SpriteRotationDeg(MakeWeapon(2, WeaponType::Cannon, Side::Black, 0, 0, 0)) == 180);
}

TEST_CASE("DirFromDeg is exact on multiples of 45") {
    CHECK(DirFromDeg(-90).x == 0);
    CHECK(DirFromDeg(-90).y == -1);
    CHECK(DirFromDeg(-45).x == DirFromDeg(45).x);
    CHECK(DirFromDeg(-45).y == -DirFromDeg(45).y);
    CHECK(DirFromDeg(-135).x == -DirFromDeg(-45).x);
    CHECK(DirFromDeg(30).x == doctest::Approx(0.8660254));
}
