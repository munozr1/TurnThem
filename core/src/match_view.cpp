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
    projectiles_.clear();
    ghosts_.clear();
    for (int i = 0; i < kHandSize; i++) hand_[i] = {start.hand[i], false};
    elixir_ = start.start_elixir;
    buffered_.clear();
    early_hits_.clear();
    result_.reset();
    rejection_.reset();
    applied_tick_ = -1;
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
            if (auto* hit = std::get_if<EvProjHit>(&event)) {
                auto it = std::find_if(projectiles_.begin(), projectiles_.end(),
                                       [&](const ViewProjectile& p) { return p.id == hit->proj_id; });
                if (it != projectiles_.end()) {
                    it->hit_known = true;
                    it->hit_point = hit->point;
                } else {
                    early_hits_[hit->proj_id] = hit->point;
                }
            }
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
            if (placed->side == side_) elixir_ -= CardCostPoints(placed->type);
            grid_.MarkWeapon(placed->col, placed->row);
            weapons_.push_back(MakeWeapon(placed->weapon_id, placed->type, placed->side, placed->col, placed->row, tick.tick));
            ghosts_.erase(std::remove_if(ghosts_.begin(), ghosts_.end(),
                                         [&](const Ghost& g) { return g.weapon_id == placed->weapon_id; }),
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
            // a hit always lands on a tile the shooter doesn't own, so it flips
            Tile& tile = grid_.at(hit->col, hit->row);
            tile.side = Opponent(tile.side);
            projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                                              [&](const ViewProjectile& p) { return p.id == hit->proj_id; }),
                               projectiles_.end());
        }
    }
    if (tick.has_checksum) {
        if (grid_.Checksum() == tick.checksum) checksums_verified_++;
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
    const int64_t last = std::min<int64_t>(applied_tick_, (int64_t)end_tick_ - 1);
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

bool MatchView::ResultVisible() const {
    if (!result_) return false;
    // a forfeit shows straight away; the timer result once playback reaches the end
    return result_->reason != EndReason::Timer || applied_tick_ >= (int64_t)result_->end_tick;
}

MatchView::Phase MatchView::phase() const {
    if (!started_) return Phase::Waiting;
    if (ResultVisible()) return Phase::Ended;
    if (clock_.playback() < start_tick_) return Phase::Countdown;
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

PlaceError MatchView::CheckLocal(int col, int row) const {
    if (phase() != Phase::Running) return PlaceError::NotRunning;
    const PlaceError err = ValidatePlacement(grid_, side_, col, row);
    if (err != PlaceError::Ok) return err;
    // pending placements will occupy and pad their cells too
    for (const Ghost& ghost : ghosts_) {
        const int dc = abs(col - ghost.col), dr = abs(row - ghost.row);
        if (dc == 0 && dr == 0) return PlaceError::Occupied;
        if (dc <= 1 && dr <= 1) return PlaceError::Padding;
    }
    return PlaceError::Ok;
}

bool MatchView::DoubleElixir() const {
    const double pb = clock_.playback();
    return pb >= 0 && RegenOnTick((uint32_t)pb, start_tick_, end_tick_) == 2;
}

ElixirPoints MatchView::AvailableElixir() const {
    ElixirPoints available = elixir_;
    for (const Ghost& ghost : ghosts_) available -= CardCostPoints(ghost.type);
    return std::max<ElixirPoints>(0, available);
}

PlaceError MatchView::CheckPlace(uint8_t slot, int col, int row) const {
    if (slot >= kHandSize || !hand_[slot].type || hand_[slot].pending) return PlaceError::BadSlot;
    const PlaceError err = CheckLocal(col, row);
    if (err != PlaceError::Ok) return err;
    if (!CanAfford(*hand_[slot].type)) return PlaceError::NotEnoughElixir;
    return PlaceError::Ok;
}

std::optional<MsgPlaceRequest> MatchView::RequestPlace(uint8_t slot, int col, int row) {
    if (CheckPlace(slot, col, row) != PlaceError::Ok) return std::nullopt;
    const MsgPlaceRequest request{next_req_id_++, slot, (uint8_t)col, (uint8_t)row, *hand_[slot].type};
    ghosts_.push_back({request.req_id, 0, slot, request.expected, request.col, request.row});
    hand_[slot].pending = true;
    return request;
}

} // namespace tt
