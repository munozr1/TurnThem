#include "tt/match_view.h"
#include <algorithm>
#include <math.h>
#include <stdlib.h>

namespace tt {

void MatchView::Reset(const MsgMatchStart& start, double now_s) {
    (void)now_s;
    clock_.Reset();
    started_ = true;
    match_id_ = start.match_id;
    side_ = start.your_side;
    start_tick_ = start.start_tick;
    end_tick_ = start.end_tick;
    grid_ = Grid::Initial();
    weapons_.clear();
    bases_ = MakeHeadquarters();
    infantry_.clear();
    previous_infantry_.clear();
    projectiles_.clear();
    ghosts_.clear();
    for (int i = 0; i < kHandSize; i++) hand_[i] = {start.hand[i], false};
    elixir_ = start.start_elixir;
    setup_elixir_ = start.setup_elixir;
    buffered_.clear();
    early_hits_.clear();
    result_.reset();
    rejection_.reset();
    applied_tick_ = -1;
    next_req_id_ = 1;
    checksum_mismatches_ = 0;
    checksums_verified_ = 0;
}

void MatchView::OnMessage(const ServerMsg& msg, double now_s) {
    if (auto* start = std::get_if<MsgMatchStart>(&msg)) {
        Reset(*start, now_s);
        return;
    }
    if (!started_) return;

    if (auto* tick = std::get_if<MsgTick>(&msg)) {
        clock_.OnTick(tick->tick, now_s);
        // note where projectiles will stop, so playback never draws them past it
        for (const TickEvent& event : tick->events) {
            std::visit([&](const auto& hit) {
                using T = std::decay_t<decltype(hit)>;
                if constexpr (std::is_same_v<T, EvProjHit> || std::is_same_v<T, EvWeaponHit>) {
                    if (hit.proj_id == 0) return; // direct rifle damage has no shell
                    auto it = std::find_if(projectiles_.begin(), projectiles_.end(),
                                           [&](const ViewProjectile& p) { return p.id == hit.proj_id; });
                    if (it != projectiles_.end()) {
                        it->hit_known = true;
                        it->hit_point = hit.point;
                    } else {
                        early_hits_[hit.proj_id] = hit.point;
                    }
                }
            }, event);
        }
        buffered_.push_back(*tick);
    } else if (auto* result = std::get_if<MsgPlaceResult>(&msg)) {
        auto ghost = std::find_if(ghosts_.begin(), ghosts_.end(), [&](const Ghost& g) { return g.req_id == result->req_id; });
        if (ghost == ghosts_.end()) return;
        if (result->err == PlaceError::Ok) {
            // its TICK comes first on the wire, so the weapon is either played back
            // already (a late result after a stall) or still buffered
            const bool shown = std::any_of(weapons_.begin(), weapons_.end(),
                                           [&](const WeaponState& w) { return w.id == result->weapon_id; });
            if (shown) ghosts_.erase(ghost);
            else ghost->weapon_id = result->weapon_id; // removed when the weapon is played back
        } else {
            hand_[ghost->slot].pending = false; // the card comes back
            ghosts_.erase(ghost);
            rejection_ = result->err;
        }
    } else if (auto* slot = std::get_if<MsgHandSlot>(&msg)) {
        hand_[slot->slot] = {slot->type, false};
    } else if (auto* end = std::get_if<MsgMatchEnd>(&msg)) {
        result_ = *end;
    }
}

void MatchView::ApplyTick(const MsgTick& tick) {
    // same order as the server: regenerate, then pay for this tick's placements
    elixir_ = Regenerate(elixir_, tick.tick, start_tick_, end_tick_);
    for (const TickEvent& event : tick.events) {
        if (auto* placed = std::get_if<EvWeaponPlaced>(&event)) {
            if (placed->side == side_) {
                ElixirPoints& balance = tick.tick < start_tick_ ? setup_elixir_ : elixir_;
                balance -= CardCostPoints(placed->type);
            }
            grid_.MarkWeapon(placed->col, placed->row);
            weapons_.push_back(MakeWeapon(placed->weapon_id, placed->type, placed->side, placed->col, placed->row,
                                          std::max(tick.tick, start_tick_)));
            // our ghost for this weapon, even if its PLACE_RESULT hasn't arrived yet
            // (weapon_id still 0); otherwise its cost would count twice until it does.
            // Only one of our ghosts can sit on a cell, and only its request can put
            // our weapon there.
            const bool ours = placed->side == side_;
            ghosts_.erase(std::remove_if(ghosts_.begin(), ghosts_.end(),
                                         [&](const Ghost& g) {
                                             return g.weapon_id == placed->weapon_id ||
                                                    (ours && g.weapon_id == 0 && g.col == placed->col && g.row == placed->row);
                                         }),
                          ghosts_.end());
        } else if (auto* spawn = std::get_if<EvProjSpawned>(&event)) {
            auto weapon = std::find_if(weapons_.begin(), weapons_.end(),
                                       [&](const WeaponState& w) { return w.id == spawn->weapon_id; });
            if (weapon == weapons_.end()) continue;
            ViewProjectile p{spawn->proj_id, spawn->weapon_id, weapon->side, spawn->pos, spawn->vel, tick.tick};
            auto early = early_hits_.find(p.id);
            if (early != early_hits_.end()) {
                p.hit_known = true;
                p.hit_point = early->second;
                early_hits_.erase(early);
            }
            projectiles_.push_back(p);
        } else if (auto* hit = std::get_if<EvProjHit>(&event)) {
            // Shells stop at combat targets; battlefield ownership never changes.
            projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                                              [&](const ViewProjectile& p) { return p.id == hit->proj_id; }),
                               projectiles_.end());
        } else if (auto* hit = std::get_if<EvWeaponHit>(&event)) {
            auto weapon = std::find_if(weapons_.begin(), weapons_.end(),
                                       [&](const WeaponState& w) { return w.id == hit->weapon_id; });
            if (weapon != weapons_.end()) {
                weapon->health = hit->health;
                if (weapon->health == 0) {
                    grid_.RemoveWeapon(weapon->col, weapon->row);
                    weapons_.erase(weapon);
                }
            }
            projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                                              [&](const ViewProjectile& p) { return p.id == hit->proj_id; }),
                               projectiles_.end());
        } else if (auto* squad = std::get_if<EvSquadPlaced>(&event)) {
            if (squad->side == side_) {
                elixir_ -= CardCostPoints(WeaponType::Infantry);
                ghosts_.erase(std::remove_if(ghosts_.begin(), ghosts_.end(), [&](const Ghost& ghost) {
                    return ghost.weapon_id == squad->squad_id ||
                           (ghost.weapon_id == 0 && ghost.type == WeaponType::Infantry &&
                            ghost.col == squad->col && ghost.row == squad->row);
                }), ghosts_.end());
            }
        } else if (auto* state = std::get_if<EvBattleState>(&event)) {
            previous_infantry_.clear();
            for (const InfantryState& unit : infantry_) previous_infantry_[unit.id] = unit.pos;
            bases_ = state->bases;
            infantry_ = state->infantry;
        }
    }
    if (tick.has_checksum) {
        if (BattleChecksum(grid_, weapons_, bases_, infantry_) == tick.checksum) checksums_verified_++;
        else checksum_mismatches_++;
    }
    applied_tick_ = tick.tick;
}

void MatchView::Update(double now_s) {
    if (!started_) return;
    clock_.Update(now_s);
    const double shown = floor(clock_.playback());
    while (!buffered_.empty() && buffered_.front().tick <= shown) {
        ApplyTick(buffered_.front());
        buffered_.pop_front();
    }

    // re-run weapon animation up to the tick being shown (the server stops stepping at the end)
    int64_t last = std::min<int64_t>(applied_tick_, (int64_t)end_tick_ - 1);
    if (result_) last = std::min(last, (int64_t)result_->end_tick - 1);
    // HQ damage resolves before guns fire. The lethal tick freezes survivors.
    if (bases_[0].health == 0 || bases_[1].health == 0) last = std::min(last, applied_tick_ - 1);
    for (WeaponState& weapon : weapons_) {
        while ((int64_t)weapon.placed_tick + weapon.age <= last) StepWeapon(weapon, nullptr);
    }

    // the server drops projectiles that leave the board without telling anyone
    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                                      [&](const ViewProjectile& p) { return !p.hit_known && OutOfBounds(ProjectilePos(p)); }),
                       projectiles_.end());
}

Vec2 MatchView::ProjectilePos(const ViewProjectile& p) const {
    const float t = (float)std::max(0.0, clock_.playback() - p.spawn_tick) * kDt;
    const Vec2 pos = {p.spawn.x + p.vel.x * t, p.spawn.y + p.vel.y * t};
    if (p.hit_known) {
        const float travelled = sqrtf(p.vel.x * p.vel.x + p.vel.y * p.vel.y) * t;
        const float to_hit = hypotf(p.hit_point.x - p.spawn.x, p.hit_point.y - p.spawn.y);
        if (travelled >= to_hit) return p.hit_point;
    }
    return pos;
}

Vec2 MatchView::InfantryPos(const InfantryState& soldier) const {
    const auto previous = previous_infantry_.find(soldier.id);
    if (previous == previous_infantry_.end()) return soldier.pos;
    // Interpolate the last two authoritative samples; never predict combat.
    const float fraction = (float)std::clamp(clock_.playback() - applied_tick_, 0.0, 1.0);
    return {previous->second.x + (soldier.pos.x - previous->second.x) * fraction,
            previous->second.y + (soldier.pos.y - previous->second.y) * fraction};
}

bool MatchView::ResultVisible() const {
    if (!result_) return false;
    // a forfeit shows straight away; the timer result once playback reaches the end
    return (result_->reason != EndReason::Timer && result_->reason != EndReason::BaseDestroyed) ||
           applied_tick_ >= (int64_t)result_->end_tick;
}

MatchView::Phase MatchView::phase() const {
    if (!started_) return Phase::Waiting;
    if (ResultVisible()) return Phase::Ended;
    if (clock_.playback() < start_tick_) return Phase::Setup;
    return Phase::Running;
}

std::optional<MsgMatchEnd> MatchView::result() const {
    if (!ResultVisible()) return std::nullopt;
    return result_;
}

// playback starts a few ticks below 0 (the buffer filling); count from 0 so the
// countdown doesn't open on "4" and the HUD timer doesn't read 3:01
float MatchView::SecondsLeft() const {
    const double pb = std::max(0.0, clock_.playback());
    return (float)std::max(0.0, (end_tick_ - pb) / kTickRate);
}

float MatchView::CountdownLeft() const {
    const double pb = std::max(0.0, clock_.playback());
    return (float)std::max(0.0, (start_tick_ - pb) / kTickRate);
}

std::optional<PlaceError> MatchView::TakeRejection() {
    std::optional<PlaceError> out;
    out.swap(rejection_);
    return out;
}

PlaceError MatchView::CheckDeployment(WeaponType type, int col, int row) const {
    if (phase() != Phase::Running && phase() != Phase::Setup) return PlaceError::NotRunning;
    // Apply reservations to temporary placement data using the same footprints
    // as the server: mobile squads never receive artillery's padding ring.
    Grid reserved = grid_;
    std::vector<InfantryState> infantry = infantry_;
    for (const Ghost& ghost : ghosts_) {
        if (ghost.type == WeaponType::Infantry) {
            for (Vec2 pos : SquadPositions(side_, ghost.col, ghost.row)) {
                InfantryState soldier;
                soldier.side = side_;
                soldier.pos = pos;
                infantry.push_back(soldier);
            }
        } else {
            reserved.MarkWeapon(ghost.col, ghost.row);
        }
    }
    return type == WeaponType::Infantry
        ? ValidateInfantryPlacement(reserved, side_, col, row, infantry)
        : ValidateArtilleryPlacement(reserved, side_, col, row, infantry);
}

PlaceError MatchView::CheckLocal(int col, int row) const {
    return CheckDeployment(WeaponType::Cannon, col, row);
}

bool MatchView::DoubleElixir() const {
    const double pb = clock_.playback();
    return pb >= 0 && RegenOnTick((uint32_t)pb, start_tick_, end_tick_) == 2;
}

ElixirPoints MatchView::AvailableElixir() const {
    const bool setup = phase() == Phase::Setup;
    ElixirPoints available = setup ? setup_elixir_ : elixir_;
    for (const Ghost& ghost : ghosts_) if (ghost.setup == setup) available -= CardCostPoints(ghost.type);
    return std::max<ElixirPoints>(0, available);
}

PlaceError MatchView::CheckPlace(uint8_t slot, int col, int row) const {
    if (slot >= kHandSize || !hand_[slot].type || hand_[slot].pending) return PlaceError::BadSlot;
    if (phase() != Phase::Running && phase() != Phase::Setup) return PlaceError::NotRunning;
    const WeaponType type = *hand_[slot].type;
    if (phase() == Phase::Setup && type == WeaponType::Infantry) return PlaceError::SetupArtilleryOnly;
    const PlaceError err = CheckDeployment(type, col, row);
    if (err != PlaceError::Ok) return err;
    if (!CanAfford(type)) return PlaceError::NotEnoughElixir;
    return PlaceError::Ok;
}

std::optional<MsgPlaceRequest> MatchView::RequestPlace(uint8_t slot, int col, int row) {
    if (CheckPlace(slot, col, row) != PlaceError::Ok) return std::nullopt;
    const MsgPlaceRequest request{next_req_id_++, slot, (uint8_t)col, (uint8_t)row, *hand_[slot].type, phase() == Phase::Setup};
    ghosts_.push_back({request.req_id, 0, slot, request.expected, request.col, request.row, request.setup});
    hand_[slot].pending = true;
    return request;
}

} // namespace tt
