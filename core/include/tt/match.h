#pragma once
#include <array>
#include <optional>
#include <random>
#include <variant>
#include <vector>
#include "tt/elixir.h"
#include "tt/battle.h"
#include "tt/grid.h"
#include "tt/weapons.h"

namespace tt {

using Hand = std::array<WeaponType, kHandSize>;

struct MatchConfig {
    uint64_t seed = 1;
    uint32_t countdown_ticks = 20 * kTickRate; // interactive setup
    uint32_t duration_ticks = 180 * kTickRate;
    uint32_t place_cooldown_ticks = 6;
    ElixirPoints start_elixir = kStartElixirPoints;
    ElixirPoints setup_elixir = ElixirToPoints(6);
};

enum class EndReason : uint8_t { Timer, Forfeit, Cancelled, BaseDestroyed, Count };
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
struct EvProjHit { uint32_t proj_id; uint8_t col, row; Vec2 point; }; // shell hits infantry/HQ; ground stays unchanged
struct EvWeaponHit { uint32_t proj_id, weapon_id; uint16_t health; Vec2 point; }; // proj_id 0 = rifle; zero health removes weapon
struct EvSquadPlaced { uint32_t squad_id; Side side; uint8_t col, row; };
// End-of-tick authoritative positions/health. At most 48 infantry total.
struct EvBattleState { std::array<HeadquartersState, 2> bases; std::vector<InfantryState> infantry; };
// only for `to`
struct EvPlaceResult { Side to; uint32_t req_id; PlaceError err; uint32_t weapon_id; };
struct EvHandSlot { Side to; uint8_t slot; WeaponType type; };
// end of the match
struct EvMatchEnded { EndReason reason; Winner winner; uint16_t white, black; };

using Event = std::variant<EvWeaponPlaced, EvProjSpawned, EvProjHit, EvWeaponHit, EvSquadPlaced, EvBattleState, EvPlaceResult, EvHandSlot, EvMatchEnded>;

// The authoritative simulation of one match. Runs headless at kTickRate; the
// server owns one per match and clients only ever see its events.
class Match {
public:
    explicit Match(const MatchConfig& config);

    // queued and applied, in arrival order, at the start of the next Step
    void SubmitPlace(Side side, uint32_t req_id, uint8_t slot, uint8_t col, uint8_t row, WeaponType expected,
                     std::optional<bool> setup = std::nullopt);
    // a player quit: a forfeit once play has started, otherwise the match is cancelled
    void Forfeit(Side side);

    // simulates tick(), appends what happened to `events`, then advances
    void Step(std::vector<Event>& events);

    uint32_t tick() const { return tick_; } // the tick the next Step simulates
    uint32_t start_tick() const { return config_.countdown_ticks; }
    uint32_t end_tick() const { return config_.countdown_ticks + config_.duration_ticks; }
    bool ended() const { return ended_; }
    MatchPhase phase() const { return ended_ ? MatchPhase::Ended : tick_ < start_tick() ? MatchPhase::Setup : MatchPhase::Combat; }
    const MatchConfig& config() const { return config_; }
    const Grid& grid() const { return grid_; }
    const Hand& hand(Side side) const { return hands_[(int)side]; }
    ElixirPoints elixir(Side side) const { return elixir_[(int)side]; }
    ElixirPoints setup_elixir(Side side) const { return setup_elixir_[(int)side]; }
    const std::vector<WeaponState>& weapons() const { return weapons_; }
    const std::vector<ProjectileState>& projectiles() const { return projectiles_; }
    const std::array<HeadquartersState, 2>& bases() const { return bases_; }
    const std::vector<InfantryState>& infantry() const { return infantry_; }
    uint32_t checksum() const { return BattleChecksum(grid_, weapons_, bases_, infantry_); }

private:
    struct PendingPlace {
        Side side;
        uint32_t req_id;
        uint8_t slot, col, row;
        WeaponType expected;
        bool setup;
    };

    PlaceError CheckPlace(const PendingPlace& place) const;
    WeaponType DrawCard(bool setup = false);
    void End(EndReason reason, Winner winner, std::vector<Event>& events);
    void StepInfantry(std::vector<Event>& events);

    MatchConfig config_;
    Grid grid_;
    std::mt19937_64 rng_;
    std::array<Hand, 2> hands_{};
    std::array<int64_t, 2> last_place_tick_{-1, -1};
    std::array<ElixirPoints, 2> elixir_{};
    std::array<ElixirPoints, 2> setup_elixir_{};
    std::vector<PendingPlace> pending_;
    std::vector<WeaponState> weapons_;         // placement order = update order
    std::vector<ProjectileState> projectiles_; // spawn order = update order
    std::array<HeadquartersState, 2> bases_ = MakeHeadquarters();
    std::vector<InfantryState> infantry_;
    uint32_t tick_ = 0;
    uint32_t next_weapon_id_ = 1;
    uint32_t next_proj_id_ = 1;
    int forfeit_side_ = -1;
    bool ended_ = false;
};

// true once a projectile has left the board plus kOutOfBoundsMargin
bool OutOfBounds(Vec2 p);

} // namespace tt
