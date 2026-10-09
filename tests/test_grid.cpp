#include "doctest/doctest.h"
#include "tt/grid.h"

using namespace tt;

TEST_CASE("initial grid is split down the middle") {
    const Grid grid = Grid::Initial();
    CHECK(grid.Count(Side::White) == 130);
    CHECK(grid.Count(Side::Black) == 130);
    CHECK(grid.at(0, 9).side == Side::Black);
    CHECK(grid.at(0, 10).side == Side::White);
}

TEST_CASE("MarkWeapon occupies the cell and pads its neighbours, clipped at edges") {
    Grid grid = Grid::Initial();
    grid.MarkWeapon(0, 0);
    CHECK(grid.at(0, 0).occupied);
    CHECK(!grid.at(0, 0).padding);
    CHECK(grid.at(1, 0).padding);
    CHECK(grid.at(0, 1).padding);
    CHECK(grid.at(1, 1).padding);
    CHECK(!grid.at(2, 0).padding);

    grid.MarkWeapon(6, 12);
    int padded = 0;
    for (int r = 11; r <= 13; r++)
        for (int c = 5; c <= 7; c++) padded += grid.at(c, r).padding;
    CHECK(padded == 8);
}

TEST_CASE("ValidatePlacement enforces bounds, own colour, occupied and padding") {
    Grid grid = Grid::Initial();
    CHECK(ValidatePlacement(grid, Side::White, -1, 12) == PlaceError::OutOfBounds);
    CHECK(ValidatePlacement(grid, Side::White, 13, 12) == PlaceError::OutOfBounds);
    CHECK(ValidatePlacement(grid, Side::White, 0, 20) == PlaceError::OutOfBounds);
    CHECK(ValidatePlacement(grid, Side::White, 6, 5) == PlaceError::NotOwnTile);
    CHECK(ValidatePlacement(grid, Side::Black, 6, 15) == PlaceError::NotOwnTile);
    CHECK(ValidatePlacement(grid, Side::White, 6, 15) == PlaceError::Ok);
    CHECK(ValidatePlacement(grid, Side::Black, 6, 5) == PlaceError::Ok);
    grid.MarkWeapon(6, 15);
    CHECK(ValidatePlacement(grid, Side::White, 6, 15) == PlaceError::Occupied);
    CHECK(ValidatePlacement(grid, Side::White, 7, 16) == PlaceError::Padding);
    CHECK(ValidatePlacement(grid, Side::White, 8, 15) == PlaceError::Ok);
}

TEST_CASE("CellAt floors and rejects off-board points instead of clamping") {
    int c, r;
    CHECK(CellAt({0, 0}, c, r));
    CHECK((c == 0 && r == 0));
    CHECK(CellAt({519.9f, 799.9f}, c, r));
    CHECK((c == 12 && r == 19));
    CHECK(!CellAt({520, 0}, c, r));
    CHECK(!CellAt({-0.1f, 5}, c, r));
    CHECK(!CellAt({5, 800}, c, r));
    const Vec2 center = CellCenter(6, 12);
    CHECK(center.x == 260);
    CHECK(center.y == 500);
}

TEST_CASE("Checksum tracks tile state") {
    Grid a = Grid::Initial(), b = Grid::Initial();
    CHECK(a.Checksum() == b.Checksum());
    b.at(3, 9).side = Side::White;
    CHECK(a.Checksum() != b.Checksum());
    a.at(3, 9).side = Side::White;
    CHECK(a.Checksum() == b.Checksum());
    b.MarkWeapon(3, 15);
    CHECK(a.Checksum() != b.Checksum());
}
