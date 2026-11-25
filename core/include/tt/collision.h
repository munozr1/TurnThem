#pragma once
#include <vector>
#include "tt/grid.h"
#include "tt/weapons.h"

namespace tt {

struct TileHit {
    int col, row;  // first enemy artillery cell on the path
    Vec2 point;    // where the path entered that tile
    Vec2 normal;   // face it entered through, components in {-1,0,1} ({0,0} if it started inside)
    uint32_t weapon_id = 0;
};

// Walks the grid cells crossed by the segment from -> to, in order, and reports
// the first enemy artillery. Empty ground and friendly artillery never absorb
// shells. Read-only: the caller decides what a hit does.
bool DetectTileCollision(const Grid& grid, Vec2 from, Vec2 to, Side side, TileHit& hit,
                         const std::vector<WeaponState>& weapons = {});

} // namespace tt
