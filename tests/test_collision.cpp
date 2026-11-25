#include "doctest/doctest.h"
#include "tt/collision.h"

using namespace tt;

static Grid Filled(Side side) {
    Grid grid = Grid::Initial();
    for (int r = 0; r < kRows; r++)
        for (int c = 0; c < kCols; c++) grid.at(c, r).side = side;
    return grid;
}

TEST_CASE("vertical shot converts the first black tile at the frontier") {
    const Grid grid = Grid::Initial();
    TileHit hit;
    REQUIRE(DetectTileCollision(grid, {260, 500}, {260, 100}, Side::White, hit));
    CHECK(hit.col == 6);
    CHECK(hit.row == 9);
    CHECK(hit.point.y == doctest::Approx(400));
    CHECK(hit.normal.x == 0);
    CHECK(hit.normal.y == 1);
}

TEST_CASE("short steps through own tiles don't hit; the step that crosses the frontier does") {
    const Grid grid = Grid::Initial();
    TileHit hit;
    Vec2 pos = {260, 500};
    int steps = 0;
    for (;;) {
        const Vec2 next = {pos.x, pos.y - 800.0f * kDt};
        steps++;
        if (DetectTileCollision(grid, pos, next, Side::White, hit)) break;
        pos = next;
        REQUIRE(steps < 100);
    }
    CHECK(steps == 8); // 100 px at 13.33 px per tick
    CHECK(hit.row == 9);
}

TEST_CASE("zero-length segments and the starting cell") {
    const Grid grid = Grid::Initial();
    TileHit hit;
    CHECK(!DetectTileCollision(grid, {100, 500}, {100, 500}, Side::White, hit));
    REQUIRE(DetectTileCollision(grid, {100, 100}, {100, 100}, Side::White, hit));
    CHECK((hit.col == 2 && hit.row == 2));
    CHECK(hit.normal.x == 0);
    CHECK(hit.normal.y == 0);
}

TEST_CASE("segments entirely off the board never hit") {
    const Grid grid = Filled(Side::Black);
    TileHit hit;
    CHECK(!DetectTileCollision(grid, {-40, -40}, {-10, -30}, Side::White, hit));
    CHECK(!DetectTileCollision(grid, {530, 100}, {560, 700}, Side::White, hit));
}

TEST_CASE("entering from off the board hits the edge tile with an outward normal") {
    const Grid grid = Filled(Side::Black);
    TileHit hit;
    REQUIRE(DetectTileCollision(grid, {-10, 100}, {10, 100}, Side::White, hit));
    CHECK((hit.col == 0 && hit.row == 2));
    CHECK(hit.point.x == doctest::Approx(0));
    CHECK(hit.normal.x == -1);
}

TEST_CASE("a diagonal through an exact corner checks both side cells") {
    Grid grid = Filled(Side::White);
    grid.at(2, 6).side = Side::Black; // only touched at the corner (120,280)
    TileHit hit;
    REQUIRE(DetectTileCollision(grid, {100, 300}, {140, 260}, Side::White, hit));
    CHECK((hit.col == 2 && hit.row == 6));

    // same path split exactly at the corner, as consecutive ticks would
    const bool first = DetectTileCollision(grid, {100, 300}, {120, 280}, Side::White, hit);
    const bool second = !first && DetectTileCollision(grid, {120, 280}, {140, 260}, Side::White, hit);
    CHECK((first || second));
    CHECK((hit.col == 2 && hit.row == 6));
}

TEST_CASE("a diagonal can't slip between two enemy tiles touching at a corner") {
    Grid grid = Filled(Side::White);
    grid.at(3, 7).side = Side::Black;
    grid.at(2, 6).side = Side::Black;
    TileHit hit;
    CHECK(DetectTileCollision(grid, {101, 299}, {139, 261}, Side::White, hit));
}

TEST_CASE("a long segment walks every own tile and stops at the first enemy one") {
    Grid grid = Filled(Side::White);
    grid.at(6, 2).side = Side::Black;
    TileHit hit;
    REQUIRE(DetectTileCollision(grid, {260, 780}, {260, -40}, Side::White, hit));
    CHECK((hit.col == 6 && hit.row == 2));
    CHECK(hit.point.y == doctest::Approx(120));
}

TEST_CASE("artillery takes the hit before its tile, regardless of tile ownership") {
    for (Side shooter : {Side::White, Side::Black}) {
        for (Side ground : {Side::White, Side::Black}) {
            Grid grid = Filled(shooter);
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
            CHECK(grid.at(6, 6).side == ground); // collision is read-only
        }
    }
}

TEST_CASE("shots pass friendly artillery and hit the nearest enemy in path order") {
    Grid grid = Filled(Side::White);
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

    // Enemy terrain still stops the shot before it reaches a farther weapon.
    grid.at(6, 7).side = Side::Black;
    REQUIRE(DetectTileCollision(grid, {260, 380}, {260, 0}, Side::White, hit, weapons));
    CHECK(hit.weapon_id == 0);
    CHECK(hit.row == 7);
}

TEST_CASE("friendly artillery on enemy ground is a terrain hit, never friendly fire") {
    Grid grid = Filled(Side::White);
    grid.at(6, 6).side = Side::Black;
    grid.MarkWeapon(6, 6);
    const std::vector<WeaponState> weapons = {
        MakeWeapon(4, WeaponType::Cannon, Side::White, 6, 6, 0)
    };
    TileHit hit;
    REQUIRE(DetectTileCollision(grid, {260, 260}, {260, 250}, Side::White, hit, weapons));
    CHECK(hit.weapon_id == 0);
    CHECK(hit.row == 6);
}

TEST_CASE("artillery collision covers starting cells and split diagonal corner crossings") {
    Grid grid = Filled(Side::White);
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
    CHECK(!DetectTileCollision(grid, {100, 260}, {100, 260}, Side::White, hit, weapons));
}
