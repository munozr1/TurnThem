#include "tt/battle.h"
#include <algorithm>
#include <cstring>

namespace tt {

Vec2 BaseCenter(Side side) {
    return CellCenter(kCols / 2, side == Side::White ? kRows - 2 : 1);
}

std::array<HeadquartersState, 2> MakeHeadquarters() {
    return {{{1, Side::White, BaseCenter(Side::White), kBaseMaxHealth},
             {2, Side::Black, BaseCenter(Side::Black), kBaseMaxHealth}}};
}

std::array<Vec2, kSquadSize> SquadPositions(Side side, int col, int row) {
    const Vec2 center = CellCenter(col, row);
    const float facing = side == Side::White ? 1.0f : -1.0f;
    return {{{center.x - 12.0f * facing, center.y},
             {center.x + 12.0f * facing, center.y},
             {center.x, center.y + 14.0f * facing}}};
}

PlaceError ValidateArtilleryPlacement(const Grid& grid, Side side, int col, int row,
                                      const std::vector<InfantryState>& infantry) {
    const PlaceError board = ValidatePlacement(grid, side, col, row);
    if (board != PlaceError::Ok) return board;
    for (const auto& soldier : infantry) {
        const float dx = soldier.pos.x - std::clamp(soldier.pos.x, (float)col * kCell, (float)(col + 1) * kCell);
        const float dy = soldier.pos.y - std::clamp(soldier.pos.y, (float)row * kCell, (float)(row + 1) * kCell);
        if (soldier.health > 0 && dx * dx + dy * dy < kInfantryRadius * kInfantryRadius) return PlaceError::Occupied;
    }
    return PlaceError::Ok;
}

PlaceError ValidateInfantryPlacement(const Grid& grid, Side side, int col, int row,
                                    const std::vector<InfantryState>& infantry) {
    if (!Grid::InBounds(col, row)) return PlaceError::OutOfBounds;
    if (!InDeploymentZone(side, col, row)) return PlaceError::NotOwnTile;
    int count = 0;
    for (const auto& soldier : infantry) count += soldier.side == side && soldier.health > 0;
    if (count + kSquadSize > kMaxInfantryPerSide) return PlaceError::UnitLimit;
    for (Vec2 pos : SquadPositions(side, col, row)) {
        if (pos.x < kInfantryRadius || pos.x > kBoardW - kInfantryRadius ||
            pos.y < kInfantryRadius || pos.y > kBoardH - kInfantryRadius) return PlaceError::OutOfBounds;
        // Keep every formation member entirely in its deployment half.
        if (side == Side::White ? pos.y - kInfantryRadius < kBoardH / 2.0f
                                : pos.y + kInfantryRadius > kBoardH / 2.0f) return PlaceError::NotOwnTile;
        for (int r = 0; r < kRows; r++) {
            for (int c = 0; c < kCols; c++) {
                if (!grid.at(c, r).occupied) continue;
                const float dx = pos.x - std::clamp(pos.x, (float)c * kCell, (float)(c + 1) * kCell);
                const float dy = pos.y - std::clamp(pos.y, (float)r * kCell, (float)(r + 1) * kCell);
                if (dx * dx + dy * dy < kInfantryRadius * kInfantryRadius) return PlaceError::Occupied;
            }
        }
        for (Side owner : {Side::White, Side::Black}) {
            const Vec2 base = BaseCenter(owner);
            const float dx = pos.x - base.x, dy = pos.y - base.y;
            const float radius = kBaseRadius + kInfantryRadius;
            if (dx * dx + dy * dy < radius * radius) return PlaceError::Occupied;
        }
        for (const auto& soldier : infantry) {
            const float dx = pos.x - soldier.pos.x, dy = pos.y - soldier.pos.y;
            if (soldier.health > 0 && dx * dx + dy * dy < 4 * kInfantryRadius * kInfantryRadius)
                return PlaceError::Occupied;
        }
    }
    return PlaceError::Ok;
}

uint32_t BattleChecksum(const Grid& grid, const std::vector<WeaponState>& weapons,
                        const std::array<HeadquartersState, 2>& bases,
                        const std::vector<InfantryState>& infantry) {
    uint32_t hash = 2166136261u;
    auto integer = [&](uint32_t value) {
        for (int i = 0; i < 4; i++) {
            hash = (hash ^ (value & 0xff)) * 16777619u;
            value >>= 8;
        }
    };
    auto position = [&](Vec2 pos) {
        uint32_t x, y;
        std::memcpy(&x, &pos.x, sizeof(x));
        std::memcpy(&y, &pos.y, sizeof(y));
        integer(x);
        integer(y);
    };
    integer(grid.Checksum());
    integer((uint32_t)weapons.size());
    for (const auto& w : weapons) {
        integer(w.id); integer((uint32_t)w.type); integer((uint32_t)w.side);
        integer(w.col); integer(w.row); integer(w.health);
    }
    for (const auto& base : bases) {
        integer(base.id); integer((uint32_t)base.side); position(base.pos); integer(base.health);
    }
    integer((uint32_t)infantry.size());
    for (const auto& soldier : infantry) {
        integer(soldier.id); integer((uint32_t)soldier.side); position(soldier.pos);
        integer(soldier.health); integer((uint32_t)soldier.target_kind); integer(soldier.target_id);
        integer((uint32_t)soldier.phase); integer(soldier.cooldown);
    }
    return hash;
}

} // namespace tt
