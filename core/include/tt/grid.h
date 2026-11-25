#pragma once
#include <array>
#include "tt/types.h"

namespace tt {

struct Tile {
    Side side = Side::White;
    bool occupied = false; // a weapon sits here
    bool padding = false;  // clearance around a weapon
};

class Grid {
public:
    // top half black, bottom half white
    static Grid Initial();
    static bool InBounds(int col, int row) {
        return (unsigned)col < (unsigned)kCols && (unsigned)row < (unsigned)kRows;
    }

    Tile& at(int col, int row) { return tiles_[row * kCols + col]; }
    const Tile& at(int col, int row) const { return tiles_[row * kCols + col]; }

    // occupy (col,row) and pad its 8 neighbours
    void MarkWeapon(int col, int row);
    int Count(Side side) const;
    // FNV-1a over every tile's side/occupied/padding, for desync checks
    uint32_t Checksum() const;

private:
    std::array<Tile, kCols * kRows> tiles_{};
};

Vec2 CellCenter(int col, int row);
// cell containing p; false if p is off the board (no clamping)
bool CellAt(Vec2 p, int& col, int& row);

enum class PlaceError : uint8_t {
    Ok,
    NotRunning,   // countdown or match over
    BadSlot,
    HandMismatch, // client's idea of the card in that slot is stale
    OutOfBounds,
    NotOwnTile,
    Occupied,
    Padding,
    RateLimited,
    NotEnoughElixir,
    Count
};
const char* PlaceErrorText(PlaceError err);

// board rules only (hand, timing and rate limits are the Match's job)
PlaceError ValidatePlacement(const Grid& grid, Side side, int col, int row);

} // namespace tt
