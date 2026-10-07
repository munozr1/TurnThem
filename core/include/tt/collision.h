#pragma once
#include "tt/grid.h"

namespace tt {

struct TileHit {
    int col, row;  // first tile on the path not owned by the projectile's side
    Vec2 point;    // where the path entered that tile
    Vec2 normal;   // face it entered through, components in {-1,0,1} ({0,0} if it started inside)
};

// Walks the grid cells crossed by the segment from -> to, in order, and reports
// the first one not owned by `side`. Read-only: the caller decides what a hit does.
bool DetectTileCollision(const Grid& grid, Vec2 from, Vec2 to, Side side, TileHit& hit);

} // namespace tt
