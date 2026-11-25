#include <cmath>
#include "doctest/doctest.h"
#include "tt/battle.h"

using namespace tt;

TEST_CASE("headquarters and squad formations mirror exactly between players") {
    const auto bases = MakeHeadquarters();
    CHECK(bases[0].side == Side::White);
    CHECK(bases[1].side == Side::Black);
    CHECK(bases[0].id != bases[1].id);
    CHECK(bases[0].pos.x + bases[1].pos.x == doctest::Approx(kBoardW));
    CHECK(bases[0].pos.y + bases[1].pos.y == doctest::Approx(kBoardH));
    const auto white = SquadPositions(Side::White, 3, 14);
    const auto black = SquadPositions(Side::Black, kCols - 1 - 3, kRows - 1 - 14);
    for (int i = 0; i < kSquadSize; i++) {
        CHECK(white[i].x + black[i].x == doctest::Approx(kBoardW));
        CHECK(white[i].y + black[i].y == doctest::Approx(kBoardH));
        for (int j = 0; j < i; j++)
            CHECK(std::hypot(white[i].x - white[j].x, white[i].y - white[j].y) >= 2 * kInfantryRadius);
    }
}

TEST_CASE("formation validation checks every soldier before deployment") {
    Grid grid = Grid::Initial();
    CHECK(ValidateInfantryPlacement(grid, Side::White, 3, 14, {}) == PlaceError::Ok);
    CHECK(ValidateInfantryPlacement(grid, Side::Black, 3, 14, {}) == PlaceError::NotOwnTile);
    CHECK(ValidateInfantryPlacement(grid, Side::White, -1, 14, {}) == PlaceError::OutOfBounds);
    CHECK(ValidateInfantryPlacement(grid, Side::White, 6, 18, {}) == PlaceError::Occupied);
    CHECK(ValidateInfantryPlacement(grid, Side::Black, 6, 1, {}) == PlaceError::Occupied);
    // The rear soldier's body extends over the arena edge even when its anchor cell is valid.
    CHECK(ValidateInfantryPlacement(grid, Side::White, 3, 19, {}) == PlaceError::OutOfBounds);
    CHECK(ValidateInfantryPlacement(grid, Side::Black, 3, 0, {}) == PlaceError::OutOfBounds);
    grid.MarkWeapon(3, 14);
    CHECK(ValidateInfantryPlacement(grid, Side::White, 3, 14, {}) == PlaceError::Occupied);
    grid.RemoveWeapon(3, 14);
    const auto positions = SquadPositions(Side::White, 3, 14);
    InfantryState blocker;
    blocker.id = 17;
    blocker.pos = positions[2]; // block only the third member
    CHECK(ValidateInfantryPlacement(grid, Side::White, 3, 14, {blocker}) == PlaceError::Occupied);
    blocker.health = 0;
    CHECK(ValidateInfantryPlacement(grid, Side::White, 3, 14, {blocker}) == PlaceError::Ok);
}

TEST_CASE("the infantry cap is per side and reserves room for all three soldiers") {
    const Grid grid = Grid::Initial();
    std::vector<InfantryState> infantry(kMaxInfantryPerSide - kSquadSize);
    for (size_t i = 0; i < infantry.size(); i++) {
        infantry[i].id = (uint32_t)i + 1;
        infantry[i].pos = {20, 200}; // outside the proposed formation
    }
    CHECK(ValidateInfantryPlacement(grid, Side::White, 3, 14, infantry) == PlaceError::Ok);
    infantry.push_back(infantry.back());
    infantry.back().id++;
    CHECK(ValidateInfantryPlacement(grid, Side::White, 3, 14, infantry) == PlaceError::UnitLimit);
    infantry.back().health = 0;
    CHECK(ValidateInfantryPlacement(grid, Side::White, 3, 14, infantry) == PlaceError::Ok);
    for (auto& soldier : infantry) soldier.side = Side::Black;
    CHECK(ValidateInfantryPlacement(grid, Side::White, 3, 14, infantry) == PlaceError::Ok);
}

TEST_CASE("battle checksum detects damage movement and targeting but ignores cosmetic artillery age") {
    Grid grid = Grid::Initial();
    auto bases = MakeHeadquarters();
    std::vector<WeaponState> weapons{MakeWeapon(10, WeaponType::Cannon, Side::White, 3, 14, 0)};
    InfantryState unit;
    unit.id = 20;
    unit.pos = CellCenter(8, 13);
    std::vector<InfantryState> infantry{unit};
    const uint32_t initial = BattleChecksum(grid, weapons, bases, infantry);
    weapons[0].age++;
    CHECK(BattleChecksum(grid, weapons, bases, infantry) == initial);
    weapons[0].health--;
    CHECK(BattleChecksum(grid, weapons, bases, infantry) != initial);
    weapons[0].health++;
    bases[1].health--;
    CHECK(BattleChecksum(grid, weapons, bases, infantry) != initial);
    bases[1].health++;
    infantry[0].pos.x += 1;
    CHECK(BattleChecksum(grid, weapons, bases, infantry) != initial);
    infantry[0].pos.x -= 1;
    infantry[0].target_kind = CombatTargetKind::Headquarters;
    infantry[0].target_id = bases[1].id;
    CHECK(BattleChecksum(grid, weapons, bases, infantry) != initial);
}
