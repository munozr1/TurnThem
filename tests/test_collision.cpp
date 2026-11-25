#include "doctest/doctest.h"
#include "tt/collision.h"

using namespace tt;

TEST_CASE("empty battlefield never absorbs shells, including across the deployment boundary") {
    const Grid grid = Grid::Initial();
    TileHit hit;
    CHECK_FALSE(DetectTileCollision(grid, {260, 500}, {260, 100}, Side::White, hit));
    CHECK_FALSE(DetectTileCollision(grid, {260, 100}, {260, 500}, Side::Black, hit));
    CHECK_FALSE(DetectTileCollision(grid, {100, 100}, {100, 100}, Side::White, hit));
    CHECK_FALSE(DetectTileCollision(grid, {-40, -40}, {-10, -30}, Side::White, hit));
    CHECK_FALSE(DetectTileCollision(grid, {530, 100}, {560, 700}, Side::White, hit));
}

TEST_CASE("artillery absorbs a shell independently of ground color") {
    for (Side shooter : {Side::White, Side::Black}) {
        for (Side ground : {Side::White, Side::Black}) {
            Grid grid = Grid::Initial();
            grid.at(6, 6).side = ground;
            grid.MarkWeapon(6, 6);
            const std::vector<WeaponState> weapons = {
                MakeWeapon(7, WeaponType::SCannon, Opponent(shooter), 6, 6, 0)
            };
            TileHit hit;
            REQUIRE(DetectTileCollision(grid, {260, 340}, {260, 180}, shooter, hit, weapons));
            CHECK(hit.weapon_id == 7);
            CHECK((hit.col == 6 && hit.row == 6));
            CHECK(hit.point.y == doctest::Approx(280));
            CHECK(grid.at(6, 6).side == ground);
        }
    }
}

TEST_CASE("shots pass friendly artillery and hit the nearest enemy in path order") {
    Grid grid = Grid::Initial();
    const std::vector<WeaponState> weapons = {
        MakeWeapon(1, WeaponType::Cannon, Side::Black, 6, 2, 0),
        MakeWeapon(2, WeaponType::Cannon, Side::White, 6, 8, 0),
        MakeWeapon(3, WeaponType::Cannon, Side::Black, 6, 5, 0)
    };
    for (const WeaponState& weapon : weapons) grid.MarkWeapon(weapon.col, weapon.row);
    TileHit hit;
    REQUIRE(DetectTileCollision(grid, {260, 380}, {260, 0}, Side::White, hit, weapons));
    CHECK(hit.weapon_id == 3);
    CHECK(hit.row == 5);
    grid.at(6, 7).side = Side::Black;
    REQUIRE(DetectTileCollision(grid, {260, 380}, {260, 0}, Side::White, hit, weapons));
    CHECK(hit.weapon_id == 3);
    CHECK(hit.row == 5);
}

TEST_CASE("friendly artillery never absorbs friendly shells, on either color ground") {
    Grid grid = Grid::Initial();
    grid.at(6, 6).side = Side::Black;
    grid.MarkWeapon(6, 6);
    const std::vector<WeaponState> weapons = {
        MakeWeapon(4, WeaponType::Cannon, Side::White, 6, 6, 0)
    };
    TileHit hit;
    CHECK_FALSE(DetectTileCollision(grid, {260, 260}, {260, 250}, Side::White, hit, weapons));
}

TEST_CASE("artillery collision covers starting cells and split diagonal corner crossings") {
    Grid grid = Grid::Initial();
    grid.MarkWeapon(2, 6);
    std::vector<WeaponState> weapons = {
        MakeWeapon(9, WeaponType::SCannon, Side::Black, 2, 6, 0)
    };
    TileHit hit;
    REQUIRE(DetectTileCollision(grid, {100, 260}, {100, 260}, Side::White, hit, weapons));
    CHECK(hit.weapon_id == 9);
    const bool first = DetectTileCollision(grid, {100, 300}, {120, 280}, Side::White, hit, weapons);
    const bool second = !first && DetectTileCollision(grid, {120, 280}, {140, 260}, Side::White, hit, weapons);
    REQUIRE((first || second));
    CHECK(hit.weapon_id == 9);
    CHECK((hit.col == 2 && hit.row == 6));
    weapons[0].health = 0;
    CHECK_FALSE(DetectTileCollision(grid, {100, 260}, {100, 260}, Side::White, hit, weapons));
}

TEST_CASE("a fast shell crossing the whole arena still hits the first artillery") {
    Grid grid = Grid::Initial();
    grid.MarkWeapon(6, 2);
    const std::vector<WeaponState> weapons = {MakeWeapon(5, WeaponType::Cannon, Side::Black, 6, 2, 0)};
    TileHit hit;
    REQUIRE(DetectTileCollision(grid, {260, 780}, {260, -40}, Side::White, hit, weapons));
    CHECK(hit.weapon_id == 5);
    CHECK(hit.point.y == doctest::Approx(120));
    CHECK(hit.normal.y == 1);
}
