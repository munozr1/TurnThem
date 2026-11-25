#include "tt/match.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "tt/collision.h"

namespace tt {
namespace {

float DistanceSquared(Vec2 a, Vec2 b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// Test the full swept segment so fast shells cannot skip small infantry.
bool SegmentCircle(Vec2 from, Vec2 to, Vec2 center, float radius, float& t) {
    const Vec2 offset{from.x - center.x, from.y - center.y};
    const Vec2 delta{to.x - from.x, to.y - from.y};
    const float c = offset.x * offset.x + offset.y * offset.y - radius * radius;
    if (c <= 0) { t = 0; return true; }
    const float a = delta.x * delta.x + delta.y * delta.y;
    if (a <= 0) return false;
    const float b = offset.x * delta.x + offset.y * delta.y;
    const float discriminant = b * b - a * c;
    if (discriminant < 0) return false;
    t = (-b - std::sqrt(discriminant)) / a;
    return t >= 0 && t <= 1;
}

bool CanStand(const Grid& grid, Vec2 pos) {
    if (pos.x < kInfantryRadius || pos.x > kBoardW - kInfantryRadius ||
        pos.y < kInfantryRadius || pos.y > kBoardH - kInfantryRadius) return false;
    for (int row = 0; row < kRows; row++) {
        for (int col = 0; col < kCols; col++) {
            if (!grid.at(col, row).occupied) continue;
            const float dx = pos.x - std::clamp(pos.x, (float)col * kCell, (float)(col + 1) * kCell);
            const float dy = pos.y - std::clamp(pos.y, (float)row * kCell, (float)(row + 1) * kCell);
            if (dx * dx + dy * dy < kInfantryRadius * kInfantryRadius) return false;
        }
    }
    for (Side side : {Side::White, Side::Black}) {
        const float r = kBaseRadius + kInfantryRadius;
        if (DistanceSquared(pos, BaseCenter(side)) < r * r) return false;
    }
    return true;
}

// BFS over the small existing grid. Neighbour order rotates with the owner.
bool NextWaypoint(const Grid& grid, const InfantryState& soldier, Vec2 target,
                  const std::vector<InfantryState>& infantry, Vec2& waypoint) {
    int col, row;
    if (!CellAt(soldier.pos, col, row)) return false;
    constexpr int cells = kCols * kRows;
    std::array<int, cells> previous;
    previous.fill(-1);
    std::array<int, cells> queue{};
    const int start = row * kCols + col;
    int read = 0, count = 0, goal = -1;
    queue[count++] = start;
    previous[start] = start;
    const int forward = soldier.side == Side::White ? -1 : 1;
    const int dc[4] = {0, forward, -forward, 0};
    const int dr[4] = {forward, 0, 0, -forward};
    // A soldier already firing holds its position. Route around that cell
    // rather than queue forever outside attack range behind a stationary ally.
    auto held = [&](int c, int r) {
        for (const auto& other : infantry) {
            if (other.id == soldier.id || other.side != soldier.side || other.health == 0 ||
                other.phase != InfantryPhase::Attacking) continue;
            int oc, orow;
            if (CellAt(other.pos, oc, orow) && oc == c && orow == r) return true;
        }
        return false;
    };
    while (read < count) {
        const int cell = queue[read++], c = cell % kCols, r = cell / kCols;
        if (!held(c, r) && DistanceSquared(CellCenter(c, r), target) <= kInfantryRange * kInfantryRange) {
            goal = cell;
            break;
        }
        for (int i = 0; i < 4; i++) {
            const int nc = c + dc[i], nr = r + dr[i];
            if (!Grid::InBounds(nc, nr) || grid.at(nc, nr).occupied || held(nc, nr)) continue;
            // If we share the firing ally's cell, exit sideways or retreat
            // around it. Advancing to the next row would walk through it.
            if (cell == start && held(c, r) && nc == c && nr == r + forward) continue;
            const int next = nr * kCols + nc;
            if (previous[next] >= 0) continue;
            previous[next] = cell;
            queue[count++] = next;
        }
    }
    if (goal < 0) return false;
    while (previous[goal] != start && goal != start) goal = previous[goal];
    waypoint = CellCenter(goal % kCols, goal / kCols);
    if (held(col, row) && goal % kCols != col) waypoint.y = soldier.pos.y;
    return true;
}

void AdvanceInfantry(InfantryState& soldier, Vec2 waypoint, const Grid& grid,
                     const std::vector<InfantryState>& infantry) {
    Vec2 move{waypoint.x - soldier.pos.x, waypoint.y - soldier.pos.y};
    const float length = std::hypot(move.x, move.y);
    const float step = kInfantrySpeed * kDt;
    if (length > step) { move.x *= step / length; move.y *= step / length; }
    Vec2 next{soldier.pos.x + move.x, soldier.pos.y + move.y};
    // Local separation preserves formation readability without moving others.
    for (int pass = 0; pass < 2; pass++) {
        for (const auto& other : infantry) {
            if (other.id == soldier.id || other.health == 0) continue;
            Vec2 apart{next.x - other.pos.x, next.y - other.pos.y};
            float distance = std::hypot(apart.x, apart.y);
            const float separation = 2.0f * kInfantryRadius;
            if (distance >= separation) continue;
            if (distance < 0.001f) { apart = {soldier.id < other.id ? -1.0f : 1.0f, 0}; distance = 1; }
            const float push = std::min(step, separation - distance);
            next.x += apart.x * push / distance;
            next.y += apart.y * push / distance;
        }
    }
    if (CanStand(grid, next)) soldier.pos = next;
    else if (CanStand(grid, {next.x, soldier.pos.y})) soldier.pos.x = next.x;
    else if (CanStand(grid, {soldier.pos.x, next.y})) soldier.pos.y = next.y;
}

} // namespace

bool OutOfBounds(Vec2 p) {
    return p.x < -kOutOfBoundsMargin || p.x > kBoardW + kOutOfBoundsMargin ||
           p.y < -kOutOfBoundsMargin || p.y > kBoardH + kOutOfBoundsMargin;
}

Match::Match(const MatchConfig& config) : config_(config), grid_(Grid::Initial()), rng_(config.seed) {
    config_.start_elixir = std::clamp(config.start_elixir, ElixirPoints{0}, kMaxElixirPoints);
    config_.setup_elixir = std::clamp(config.setup_elixir, ElixirPoints{0}, kMaxElixirPoints);
    elixir_ = {config_.start_elixir, config_.start_elixir};
    setup_elixir_ = {config_.setup_elixir, config_.setup_elixir};
    for (Hand& hand : hands_) hand = {WeaponType::Cannon, WeaponType::SCannon, WeaponType::Infantry, DrawCard(true)};
}

WeaponType Match::DrawCard(bool setup) {
    return (WeaponType)(rng_() % (setup ? 2u : (uint64_t)WeaponType::Count));
}

void Match::SubmitPlace(Side side, uint32_t req_id, uint8_t slot, uint8_t col, uint8_t row,
                        WeaponType expected, std::optional<bool> setup) {
    pending_.push_back({side, req_id, slot, col, row, expected, setup.value_or(tick_ < start_tick())});
}

void Match::Forfeit(Side side) {
    if (!ended_ && forfeit_side_ < 0) forfeit_side_ = (int)side;
}

PlaceError Match::CheckPlace(const PendingPlace& place) const {
    const bool setup = tick_ < start_tick();
    if (place.setup != setup) return PlaceError::NotRunning;
    if (place.slot >= kHandSize) return PlaceError::BadSlot;
    if (hand(place.side)[place.slot] != place.expected) return PlaceError::HandMismatch;
    if (setup && place.expected == WeaponType::Infantry) return PlaceError::SetupArtilleryOnly;
    const int64_t last = last_place_tick_[(int)place.side];
    if (last >= 0 && (int64_t)tick_ - last < (int64_t)config_.place_cooldown_ticks) return PlaceError::RateLimited;
    const PlaceError err = place.expected == WeaponType::Infantry
        ? ValidateInfantryPlacement(grid_, place.side, place.col, place.row, infantry_)
        : ValidateArtilleryPlacement(grid_, place.side, place.col, place.row, infantry_);
    if (err != PlaceError::Ok) return err;
    const ElixirPoints available = setup ? setup_elixir(place.side) : elixir(place.side);
    if (available < CardCostPoints(place.expected)) return PlaceError::NotEnoughElixir;
    return PlaceError::Ok;
}

void Match::End(EndReason reason, Winner winner, std::vector<Event>& events) {
    ended_ = true;
    pending_.clear();
    events.push_back(EvMatchEnded{reason, winner, bases_[0].health, bases_[1].health});
}

void Match::StepInfantry(std::vector<Event>& events) {
    // Resolve stable IDs each time; erasure cannot leave a dangling target.
    auto target_position = [&](const InfantryState& soldier, Vec2& pos) {
        if (soldier.target_kind == CombatTargetKind::Weapon) {
            for (const auto& weapon : weapons_) {
                if (weapon.id == soldier.target_id && weapon.side != soldier.side && weapon.health > 0) {
                    pos = CellCenter(weapon.col, weapon.row); return true;
                }
            }
        } else if (soldier.target_kind == CombatTargetKind::Infantry) {
            for (const auto& other : infantry_) {
                if (other.id == soldier.target_id && other.side != soldier.side && other.health > 0) {
                    pos = other.pos; return true;
                }
            }
        } else if (soldier.target_kind == CombatTargetKind::Headquarters) {
            for (const auto& base : bases_) {
                if (base.id == soldier.target_id && base.side != soldier.side && base.health > 0) {
                    pos = base.pos; return true;
                }
            }
        }
        return false;
    };
    for (auto& soldier : infantry_) {
        if (soldier.health == 0) continue;
        if (soldier.cooldown > 0) soldier.cooldown--;
        Vec2 target{};
        bool valid = target_position(soldier, target);
        if (valid && soldier.target_kind != CombatTargetKind::Headquarters &&
            DistanceSquared(soldier.pos, target) > 4 * kInfantryAcquireRange * kInfantryAcquireRange) valid = false;
        // HQ is the march objective until a local enemy is encountered. An
        // engaged soldier/gun stays targeted until dead or beyond pursuit range.
        if (!valid || (soldier.target_kind == CombatTargetKind::Headquarters &&
                        DistanceSquared(soldier.pos, target) > kInfantryRange * kInfantryRange)) {
            float nearest = kInfantryAcquireRange * kInfantryAcquireRange;
            CombatTargetKind kind = CombatTargetKind::None;
            uint32_t id = std::numeric_limits<uint32_t>::max();
            auto consider = [&](CombatTargetKind candidate_kind, uint32_t candidate_id, Vec2 pos) {
                const float distance = DistanceSquared(soldier.pos, pos);
                if (distance < nearest || (distance == nearest && candidate_id < id)) {
                    nearest = distance; kind = candidate_kind; id = candidate_id; target = pos;
                }
            };
            for (const auto& weapon : weapons_)
                if (weapon.side != soldier.side && weapon.health > 0)
                    consider(CombatTargetKind::Weapon, weapon.id, CellCenter(weapon.col, weapon.row));
            for (const auto& other : infantry_)
                if (other.side != soldier.side && other.health > 0)
                    consider(CombatTargetKind::Infantry, other.id, other.pos);
            if (kind == CombatTargetKind::None) {
                const auto& base = bases_[(int)Opponent(soldier.side)];
                kind = CombatTargetKind::Headquarters; id = base.id; target = base.pos;
            }
            soldier.target_kind = kind;
            soldier.target_id = id;
        }
        if (DistanceSquared(soldier.pos, target) > kInfantryRange * kInfantryRange) {
            soldier.phase = InfantryPhase::Advancing;
            Vec2 waypoint;
            if (NextWaypoint(grid_, soldier, target, infantry_, waypoint)) AdvanceInfantry(soldier, waypoint, grid_, infantry_);
            else { soldier.target_kind = CombatTargetKind::None; soldier.target_id = 0; }
            continue;
        }
        soldier.phase = InfantryPhase::Attacking;
        if (soldier.cooldown != 0) continue;
        soldier.cooldown = kInfantryAttackTicks;
        if (soldier.target_kind == CombatTargetKind::Weapon) {
            auto weapon = std::find_if(weapons_.begin(), weapons_.end(), [&](const WeaponState& w) { return w.id == soldier.target_id; });
            if (weapon == weapons_.end()) continue;
            weapon->health = (uint16_t)std::max(0, (int)weapon->health - kInfantryDamage);
            events.push_back(EvWeaponHit{0, weapon->id, weapon->health, target});
            if (weapon->health == 0) { grid_.RemoveWeapon(weapon->col, weapon->row); weapons_.erase(weapon); }
        } else if (soldier.target_kind == CombatTargetKind::Infantry) {
            auto other = std::find_if(infantry_.begin(), infantry_.end(), [&](const InfantryState& u) { return u.id == soldier.target_id; });
            if (other != infantry_.end()) other->health = (uint16_t)std::max(0, (int)other->health - kInfantryDamage);
        } else if (soldier.target_kind == CombatTargetKind::Headquarters) {
            auto& base = bases_[(int)Opponent(soldier.side)];
            base.health = (uint16_t)std::max(0, (int)base.health - kInfantryDamage);
        }
    }
    // A later soldier may kill a target an earlier soldier used this tick.
    // Publish only live locks, rather than exposing stale IDs for one frame.
    for (auto& soldier : infantry_) {
        Vec2 target;
        if (soldier.health > 0 && !target_position(soldier, target)) {
            soldier.target_kind = CombatTargetKind::None;
            soldier.target_id = 0;
            soldier.phase = InfantryPhase::Advancing;
        }
    }
    infantry_.erase(std::remove_if(infantry_.begin(), infantry_.end(), [](const InfantryState& u) { return u.health == 0; }), infantry_.end());
}

void Match::Step(std::vector<Event>& events) {
    if (ended_) return;
    for (ElixirPoints& points : elixir_) points = Regenerate(points, tick_, start_tick(), end_tick());
    if (forfeit_side_ >= 0) {
        if (tick_ < start_tick()) End(EndReason::Cancelled, Winner::Draw, events);
        else End(EndReason::Forfeit, Opponent((Side)forfeit_side_) == Side::White ? Winner::White : Winner::Black, events);
        tick_++;
        return;
    }
    if (tick_ >= end_tick()) {
        End(EndReason::Timer, Winner::Draw, events);
        tick_++;
        return;
    }
    // 1. Deployments are atomic and processed in arrival order.
    for (const PendingPlace& place : pending_) {
        const PlaceError err = CheckPlace(place);
        uint32_t deployment_id = 0;
        if (err == PlaceError::Ok) {
            deployment_id = next_weapon_id_++;
            if (place.expected == WeaponType::Infantry) {
                for (Vec2 pos : SquadPositions(place.side, place.col, place.row)) {
                    InfantryState soldier;
                    soldier.id = next_weapon_id_++;
                    soldier.side = place.side;
                    soldier.pos = pos;
                    infantry_.push_back(soldier);
                }
                events.push_back(EvSquadPlaced{deployment_id, place.side, place.col, place.row});
            } else {
                grid_.MarkWeapon(place.col, place.row);
                weapons_.push_back(MakeWeapon(deployment_id, place.expected, place.side, place.col, place.row, std::max(tick_, start_tick())));
                events.push_back(EvWeaponPlaced{deployment_id, place.expected, place.side, place.col, place.row});
            }
            last_place_tick_[(int)place.side] = tick_;
            (place.setup ? setup_elixir_ : elixir_)[(int)place.side] -= CardCostPoints(place.expected);
        }
        events.push_back(EvPlaceResult{place.side, place.req_id, err, deployment_id});
        if (err == PlaceError::Ok) {
            WeaponType& card = hands_[(int)place.side][place.slot];
            card = DrawCard(place.setup);
            events.push_back(EvHandSlot{place.side, place.slot, card});
        }
    }
    pending_.clear();
    if (tick_ < start_tick()) {
        events.push_back(EvBattleState{bases_, infantry_});
        tick_++;
        return;
    }
    // 2. Shells sweep in spawn order; empty ground never absorbs them.
    size_t keep = 0;
    for (const auto& projectile : projectiles_) {
        ProjectileState p = projectile;
        const Vec2 prev = p.pos;
        p.pos = {p.pos.x + p.vel.x * kDt, p.pos.y + p.vel.y * kDt};
        CombatTargetKind kind = CombatTargetKind::None;
        uint32_t target_id = 0;
        float nearest = 2.0f;
        TileHit artillery;
        if (DetectTileCollision(grid_, prev, p.pos, p.side, artillery, weapons_)) {
            const float length = std::sqrt(DistanceSquared(prev, p.pos));
            nearest = length > 0 ? std::sqrt(DistanceSquared(prev, artillery.point)) / length : 0;
            kind = CombatTargetKind::Weapon;
            target_id = artillery.weapon_id;
        }
        auto check_circle = [&](CombatTargetKind candidate, uint32_t id, Vec2 pos, float radius) {
            float t;
            if (SegmentCircle(prev, p.pos, pos, radius, t) && t < nearest) {
                nearest = t; kind = candidate; target_id = id;
            }
        };
        for (const auto& soldier : infantry_)
            if (soldier.side != p.side && soldier.health > 0)
                check_circle(CombatTargetKind::Infantry, soldier.id, soldier.pos, kInfantryRadius);
        for (const auto& base : bases_)
            if (base.side != p.side && base.health > 0)
                check_circle(CombatTargetKind::Headquarters, base.id, base.pos, kBaseRadius);
        if (kind != CombatTargetKind::None) {
            const Vec2 point{prev.x + (p.pos.x - prev.x) * nearest, prev.y + (p.pos.y - prev.y) * nearest};
            if (kind == CombatTargetKind::Weapon) {
                auto weapon = std::find_if(weapons_.begin(), weapons_.end(), [&](const WeaponState& w) { return w.id == target_id; });
                weapon->health = (uint16_t)std::max(0, (int)weapon->health - kProjectileDamage);
                events.push_back(EvWeaponHit{p.id, weapon->id, weapon->health, point});
                if (weapon->health == 0) { grid_.RemoveWeapon(weapon->col, weapon->row); weapons_.erase(weapon); }
            } else {
                if (kind == CombatTargetKind::Infantry) {
                    auto soldier = std::find_if(infantry_.begin(), infantry_.end(), [&](const InfantryState& u) { return u.id == target_id; });
                    soldier->health = (uint16_t)std::max(0, (int)soldier->health - kProjectileDamage);
                } else {
                    auto& base = bases_[(int)Opponent(p.side)];
                    base.health = (uint16_t)std::max(0, (int)base.health - kProjectileDamage);
                }
                int col, row;
                CellAt(point, col, row);
                events.push_back(EvProjHit{p.id, (uint8_t)col, (uint8_t)row, point});
            }
            continue;
        }
        if (!OutOfBounds(p.pos)) projectiles_[keep++] = p;
    }
    projectiles_.resize(keep);
    // 3. Infantry fights before surviving guns fire; dead entities never act.
    StepInfantry(events);
    if (bases_[0].health == 0 || bases_[1].health == 0) {
        events.push_back(EvBattleState{bases_, infantry_});
        const Winner winner = bases_[0].health == 0 && bases_[1].health == 0 ? Winner::Draw
                            : bases_[0].health == 0 ? Winner::Black : Winner::White;
        End(EndReason::BaseDestroyed, winner, events);
        tick_++;
        return;
    }
    // 4. Existing stationary gun patterns and timing remain shared with view.
    for (WeaponState& weapon : weapons_) {
        Shot shot;
        if (StepWeapon(weapon, &shot)) {
            const ProjectileState p{next_proj_id_++, weapon.id, weapon.side, shot.pos, shot.vel, tick_};
            projectiles_.push_back(p);
            events.push_back(EvProjSpawned{p.id, p.weapon_id, p.pos, p.vel});
        }
    }
    events.push_back(EvBattleState{bases_, infantry_});
    tick_++;
}

} // namespace tt
