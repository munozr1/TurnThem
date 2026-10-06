#pragma once
#include "common.h"

struct TileHit {
    Block* block;   // first tile on the path not owned by the projectile's side
    Vector2 point;  // where the path entered that tile
    Vector2 normal; // face it entered through, components in {-1,0,1} ({0,0} if it started inside)
};

// Walks the grid cells crossed by the segment from -> to, in order, and reports
// the first one not owned by `side`. Read-only: the caller decides what a hit does.
bool DetectTileCollision(Vector2 from, Vector2 to, BlockSide side, TileHit& hit);
