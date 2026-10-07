#include "tt/grid.h"
#include <math.h>

namespace tt {

Grid Grid::Initial() {
    Grid grid;
    for (int row = 0; row < kRows; row++) {
        for (int col = 0; col < kCols; col++) {
            grid.at(col, row).side = (row >= kRows / 2) ? Side::White : Side::Black;
        }
    }
    return grid;
}

void Grid::MarkWeapon(int col, int row) {
    if (!InBounds(col, row)) return;
    at(col, row).occupied = true;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if ((dc != 0 || dr != 0) && InBounds(col + dc, row + dr)) {
                at(col + dc, row + dr).padding = true;
            }
        }
    }
}

int Grid::Count(Side side) const {
    int count = 0;
    for (const Tile& tile : tiles_) count += tile.side == side;
    return count;
}

uint32_t Grid::Checksum() const {
    uint32_t hash = 2166136261u;
    for (const Tile& tile : tiles_) {
        const uint8_t bits = (uint8_t)tile.side | (tile.occupied << 1) | (tile.padding << 2);
        hash = (hash ^ bits) * 16777619u;
    }
    return hash;
}

Vec2 CellCenter(int col, int row) {
    return {col * (float)kCell + kCell / 2.0f, row * (float)kCell + kCell / 2.0f};
}

bool CellAt(Vec2 p, int& col, int& row) {
    col = (int)floorf(p.x / kCell);
    row = (int)floorf(p.y / kCell);
    return Grid::InBounds(col, row);
}

const char* PlaceErrorText(PlaceError err) {
    switch (err) {
        case PlaceError::Ok: return "ok";
        case PlaceError::NotRunning: return "match isn't running";
        case PlaceError::BadSlot: return "no such card";
        case PlaceError::HandMismatch: return "card changed";
        case PlaceError::OutOfBounds: return "off the board";
        case PlaceError::NotOwnTile: return "not your tile";
        case PlaceError::Occupied: return "tile occupied";
        case PlaceError::Padding: return "too close to a weapon";
        case PlaceError::RateLimited: return "too fast";
        case PlaceError::Count: break;
    }
    return "unknown";
}

PlaceError ValidatePlacement(const Grid& grid, Side side, int col, int row) {
    if (!Grid::InBounds(col, row)) return PlaceError::OutOfBounds;
    const Tile& tile = grid.at(col, row);
    if (tile.side != side) return PlaceError::NotOwnTile;
    if (tile.occupied) return PlaceError::Occupied;
    if (tile.padding) return PlaceError::Padding;
    return PlaceError::Ok;
}

} // namespace tt
