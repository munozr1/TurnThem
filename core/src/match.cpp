#include "tt/match.h"
#include "tt/collision.h"

namespace tt {

bool OutOfBounds(Vec2 p) {
    return p.x < -kOutOfBoundsMargin || p.x > kBoardW + kOutOfBoundsMargin ||
           p.y < -kOutOfBoundsMargin || p.y > kBoardH + kOutOfBoundsMargin;
}

Match::Match(const MatchConfig& config) : config_(config), grid_(Grid::Initial()), rng_(config.seed) {
    elixir_ = {config.start_elixir, config.start_elixir};
    for (Hand& hand : hands_) {
        for (WeaponType& card : hand) card = DrawCard();
    }
}

WeaponType Match::DrawCard() {
    return (WeaponType)(rng_() % (uint64_t)WeaponType::Count);
}

void Match::SubmitPlace(Side side, uint32_t req_id, uint8_t slot, uint8_t col, uint8_t row, WeaponType expected) {
    pending_.push_back({side, req_id, slot, col, row, expected});
}

void Match::Forfeit(Side side) {
    if (!ended_ && forfeit_side_ < 0) forfeit_side_ = (int)side;
}

PlaceError Match::CheckPlace(const PendingPlace& place) const {
    if (tick_ < start_tick()) return PlaceError::NotRunning;
    if (place.slot >= kHandSize) return PlaceError::BadSlot;
    if (hand(place.side)[place.slot] != place.expected) return PlaceError::HandMismatch;
    const int64_t last = last_place_tick_[(int)place.side];
    if (last >= 0 && (int64_t)tick_ - last < (int64_t)config_.place_cooldown_ticks) return PlaceError::RateLimited;
    const PlaceError err = ValidatePlacement(grid_, place.side, place.col, place.row);
    if (err != PlaceError::Ok) return err;
    if (elixir(place.side) < CardCostPoints(place.expected)) return PlaceError::NotEnoughElixir;
    return PlaceError::Ok;
}

void Match::End(EndReason reason, Winner winner, std::vector<Event>& events) {
    ended_ = true;
    pending_.clear();
    events.push_back(EvMatchEnded{reason, winner, (uint16_t)grid_.Count(Side::White), (uint16_t)grid_.Count(Side::Black)});
}

void Match::Step(std::vector<Event>& events) {
    if (ended_) return;

    if (forfeit_side_ >= 0) {
        if (tick_ < start_tick()) {
            End(EndReason::Cancelled, Winner::Draw, events);
        } else {
            End(EndReason::Forfeit, Opponent((Side)forfeit_side_) == Side::White ? Winner::White : Winner::Black, events);
        }
        tick_++;
        return;
    }
    if (tick_ >= end_tick()) {
        const int white = grid_.Count(Side::White), black = grid_.Count(Side::Black);
        End(EndReason::Timer, white > black ? Winner::White : black > white ? Winner::Black : Winner::Draw, events);
        tick_++;
        return;
    }

    // 1. elixir, then placements in arrival order (so a card is playable on the
    //    tick its elixir arrives; the client mirrors this order)
    for (ElixirPoints& points : elixir_) points = Regenerate(points, tick_, start_tick(), end_tick());
    for (const PendingPlace& place : pending_) {
        const PlaceError err = CheckPlace(place);
        uint32_t weapon_id = 0;
        if (err == PlaceError::Ok) {
            weapon_id = next_weapon_id_++;
            grid_.MarkWeapon(place.col, place.row);
            weapons_.push_back(MakeWeapon(weapon_id, place.expected, place.side, place.col, place.row, tick_));
            last_place_tick_[(int)place.side] = tick_;
            elixir_[(int)place.side] -= CardCostPoints(place.expected);
            events.push_back(EvWeaponPlaced{weapon_id, place.expected, place.side, place.col, place.row});
        }
        events.push_back(EvPlaceResult{place.side, place.req_id, err, weapon_id});
        if (err == PlaceError::Ok) {
            WeaponType& card = hands_[(int)place.side][place.slot];
            card = DrawCard();
            events.push_back(EvHandSlot{place.side, place.slot, card});
        }
    }
    pending_.clear();

    // 2. projectiles, in spawn order: sweep this tick's movement, convert the first enemy tile
    size_t keep = 0;
    for (size_t i = 0; i < projectiles_.size(); i++) {
        ProjectileState p = projectiles_[i];
        const Vec2 prev = p.pos;
        p.pos = {p.pos.x + p.vel.x * kDt, p.pos.y + p.vel.y * kDt};
        TileHit hit;
        if (DetectTileCollision(grid_, prev, p.pos, p.side, hit)) {
            grid_.at(hit.col, hit.row).side = p.side;
            events.push_back(EvProjHit{p.id, (uint8_t)hit.col, (uint8_t)hit.row, hit.point});
            continue;
        }
        if (OutOfBounds(p.pos)) continue; // clients cull these themselves
        projectiles_[keep++] = p;
    }
    projectiles_.resize(keep);

    // 3. weapons, in placement order (one placed this tick fires this tick; its shot moves next tick)
    for (WeaponState& weapon : weapons_) {
        Shot shot;
        if (StepWeapon(weapon, &shot)) {
            const ProjectileState p{next_proj_id_++, weapon.id, weapon.side, shot.pos, shot.vel, tick_};
            projectiles_.push_back(p);
            events.push_back(EvProjSpawned{p.id, p.weapon_id, p.pos, p.vel});
        }
    }

    tick_++;
}

} // namespace tt
