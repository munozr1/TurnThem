#pragma once
#include <array>
#include <random>
#include <variant>
#include <vector>
#include "tt/elixir.h"
#include "tt/grid.h"
#include "tt/weapons.h"

namespace tt {

using Hand = std::array<WeaponType, kHandSize>;

struct MatchConfig {
    uint64_t seed = 1;
    uint32_t countdown_ticks = 3 * kTickRate;
    uint32_t duration_ticks = 180 * kTickRate;
    uint32_t place_cooldown_ticks = 6;
    ElixirPoints start_elixir = kStartElixirPoints;
};

enum class EndReason : uint8_t { Timer, Forfeit, Cancelled, Count };
enum class Winner : uint8_t { White, Black, Draw, Count };

struct ProjectileState {
    uint32_t id;
    uint32_t weapon_id;
    Side side;
    Vec2 pos;
    Vec2 vel; // px/s
    uint32_t spawn_tick;
};

// seen by both players
struct EvWeaponPlaced { uint32_t weapon_id; WeaponType type; Side side; uint8_t col, row; };
struct EvProjSpawned { uint32_t proj_id; uint32_t weapon_id; Vec2 pos; Vec2 vel; };
struct EvProjHit { uint32_t proj_id; uint8_t col, row; Vec2 point; }; // tile becomes the shooter's color
// only for `to`
struct EvPlaceResult { Side to; uint32_t req_id; PlaceError err; uint32_t weapon_id; };
struct EvHandSlot { Side to; uint8_t slot; WeaponType type; };
// end of the match
struct EvMatchEnded { EndReason reason; Winner winner; uint16_t white, black; };

using Event = std::variant<EvWeaponPlaced, EvProjSpawned, EvProjHit, EvPlaceResult, EvHandSlot, EvMatchEnded>;

// The authoritative simulation of one match. Runs headless at kTickRate; the
// server owns one per match and clients only ever see its events.
class Match {
public:
    explicit Match(const MatchConfig& config);

    // queued and applied, in arrival order, at the start of the next Step
    void SubmitPlace(Side side, uint32_t req_id, uint8_t slot, uint8_t col, uint8_t row, WeaponType expected);
    // a player quit: a forfeit once play has started, otherwise the match is cancelled
    void Forfeit(Side side);

    // simulates tick(), appends what happened to `events`, then advances
    void Step(std::vector<Event>& events);

    uint32_t tick() const { return tick_; } // the tick the next Step simulates
    uint32_t start_tick() const { return config_.countdown_ticks; }
    uint32_t end_tick() const { return config_.countdown_ticks + config_.duration_ticks; }
    bool ended() const { return ended_; }
    const MatchConfig& config() const { return config_; }
    const Grid& grid() const { return grid_; }
    const Hand& hand(Side side) const { return hands_[(int)side]; }
    ElixirPoints elixir(Side side) const { return elixir_[(int)side]; }
    const std::vector<WeaponState>& weapons() const { return weapons_; }
    const std::vector<ProjectileState>& projectiles() const { return projectiles_; }

private:
    struct PendingPlace {
        Side side;
        uint32_t req_id;
        uint8_t slot, col, row;
        WeaponType expected;
    };

    PlaceError CheckPlace(const PendingPlace& place) const;
    WeaponType DrawCard();
    void End(EndReason reason, Winner winner, std::vector<Event>& events);

    MatchConfig config_;
    Grid grid_;
    std::mt19937_64 rng_;
    std::array<Hand, 2> hands_{};
    std::array<int64_t, 2> last_place_tick_{-1, -1};
    std::array<ElixirPoints, 2> elixir_{};
    std::vector<PendingPlace> pending_;
    std::vector<WeaponState> weapons_;         // placement order = update order
    std::vector<ProjectileState> projectiles_; // spawn order = update order
    uint32_t tick_ = 0;
    uint32_t next_weapon_id_ = 1;
    uint32_t next_proj_id_ = 1;
    int forfeit_side_ = -1;
    bool ended_ = false;
};

// true once a projectile has left the board plus kOutOfBoundsMargin
bool OutOfBounds(Vec2 p);

} // namespace tt
