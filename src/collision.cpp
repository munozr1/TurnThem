#include "collision.h"
#include <math.h>
#include <stdlib.h>

// a column crossing and a row crossing closer together than this (px, along the
// major axis) count as one crossing through the corner between them
static const float CORNER_EPSILON = 0.01f;

/*
 * Grid traversal (Amanatides & Woo, "A Fast Voxel Traversal Algorithm for Ray
 * Tracing", 1987). Instead of testing the projectile against every tile, step
 * from the cell holding `from` to the cell holding `to` one boundary at a time,
 * in the order the path crosses them. Costs one array read per cell crossed
 * (1-2 per frame at normal speeds) and skips nothing in between, so a fast
 * shell or a long frame can't tunnel through a tile.
 */
bool DetectTileCollision(Vector2 from, Vector2 to, BlockSide side, TileHit& hit) {
    const float cell = (float)CELL_SIZE;
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;

    // floorf, not an int cast, so positions left of / above the grid land in cell -1, not 0
    int col = (int)floorf(from.x / cell);
    int row = (int)floorf(from.y / cell);
    const int step_col = (dx > 0) - (dx < 0);
    const int step_row = (dy > 0) - (dy < 0);

    // exact number of column/row boundaries between the two end cells. Stepping
    // by these counts (instead of until t > 1) always ends on the end cell, no
    // matter how float error lands near a boundary.
    int cols_left = abs((int)floorf(to.x / cell) - col);
    int rows_left = abs((int)floorf(to.y / cell) - row);

    // t in [0,1] along the segment where it crosses the next column/row boundary,
    // and how much t one whole cell takes. Divisions happen once here, not per step.
    float t_next_col = INFINITY, t_delta_col = INFINITY;
    float t_next_row = INFINITY, t_delta_row = INFINITY;
    if (step_col != 0) {
        t_next_col = ((step_col > 0 ? col + 1 : col) * cell - from.x) / dx;
        t_delta_col = cell / fabsf(dx);
    }
    if (step_row != 0) {
        t_next_row = ((step_row > 0 ? row + 1 : row) * cell - from.y) / dy;
        t_delta_row = cell / fabsf(dy);
    }
    const float span = fmaxf(fabsf(dx), fabsf(dy)); // turns a t difference into ~pixels

    // checks one cell; fills `hit` if it belongs to the other side
    auto is_hit = [&](int c, int r, float t, float nx, float ny) {
        // unsigned compare rejects negative and too-large indices in one test per axis
        if ((unsigned)c >= (unsigned)GRID_COLS || (unsigned)r >= (unsigned)GRID_ROWS) return false;
        Block& block = blocks[r * GRID_COLS + c]; // row-major, same layout InitGrid builds
        if (block.side == side) return false;
        t = fminf(fmaxf(t, 0.0f), 1.0f);
        hit.block = &block;
        hit.point = {from.x + dx * t, from.y + dy * t};
        hit.normal = {nx, ny};
        return true;
    };

    // the starting cell too: an enemy shot may have flipped it while we were inside
    if (is_hit(col, row, 0.0f, 0.0f, 0.0f)) return true;

    while (cols_left > 0 || rows_left > 0) {
        const bool corner = fabsf(t_next_col - t_next_row) * span < CORNER_EPSILON;
        if (corner && cols_left > 0 && rows_left > 0) {
            // through a corner (45 degree scannon shots from a cell center hit one every
            // cell). Both side cells are touched at the same instant, so check both
            // instead of letting float noise pick one, then move diagonally.
            const float t = fminf(t_next_col, t_next_row);
            if (is_hit(col, row + step_row, t, 0.0f, (float)-step_row)) return true;
            if (is_hit(col + step_col, row, t, (float)-step_col, 0.0f)) return true;
            col += step_col;
            row += step_row;
            cols_left--;
            rows_left--;
            t_next_col += t_delta_col;
            t_next_row += t_delta_row;
            if (is_hit(col, row, t, (float)-step_col, (float)-step_row)) return true;
        } else if (rows_left == 0 || (cols_left > 0 && t_next_col < t_next_row)) {
            // `corner` here means the segment ends right at a corner, so its other
            // crossing falls in the next frame's call. Check that side cell now, in the
            // same order as above, or the frame split would skip it.
            const float t = t_next_col;
            if (corner && is_hit(col, row + step_row, t, 0.0f, (float)-step_row)) return true;
            col += step_col;
            cols_left--;
            t_next_col += t_delta_col;
            if (is_hit(col, row, t, (float)-step_col, 0.0f)) return true;
        } else {
            const float t = t_next_row;
            row += step_row;
            rows_left--;
            t_next_row += t_delta_row;
            if (is_hit(col, row, t, 0.0f, (float)-step_row)) return true;
            if (corner && is_hit(col + step_col, row - step_row, t, (float)-step_col, 0.0f)) return true;
        }
    }
    return false;
}
