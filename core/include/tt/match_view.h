#pragma once
#include <array>
#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>
#include "tt/elixir.h"
#include "tt/playback_clock.h"
#include "tt/protocol.h"

namespace tt {

struct ViewProjectile {
    uint32_t id, weapon_id;
    Side side;
    Vec2 spawn, vel;
    uint32_t spawn_tick;
    bool hit_known = false; // a buffered PROJ_HIT says where it stops
    Vec2 hit_point{};
};

// a placement we've asked for but haven't seen played back yet
struct Ghost {
    uint32_t req_id;
    uint32_t weapon_id = 0; // known once PLACE_RESULT accepts it
    uint8_t slot;
    WeaponType type;
    uint8_t col, row;
};

struct HandSlotView {
    std::optional<WeaponType> type;
    bool pending = false; // spent on a request the server hasn't answered yet
};

// The client's copy of a match: server messages played back `delay` ticks
// behind the server. Tiles only change when a server event is played back;
// weapon animation is re-run locally from the same state machine as the server
// (cosmetic only), and projectiles move analytically from their spawn.
// No rendering here, so the game client and the headless bot share it.
class MatchView {
public:
    enum class Phase { Waiting, Countdown, Running, Ended };

    explicit MatchView(double delay_ticks = 6.0) : clock_(delay_ticks) {}

    void OnMessage(const ServerMsg& msg, double now_s);
    void Update(double now_s);

    // why (col,row) can't take a weapon right now, from what this client knows (board + pending)
    PlaceError CheckLocal(int col, int row) const;
    // CheckLocal plus the card itself: the slot holds a playable card and we can afford it
    PlaceError CheckPlace(uint8_t slot, int col, int row) const;
    // checks locally, records a ghost and marks the slot pending; returns the request to send
    std::optional<MsgPlaceRequest> RequestPlace(uint8_t slot, int col, int row);

    Phase phase() const;
    Side side() const { return side_; }
    uint32_t match_id() const { return match_id_; }
    const Grid& grid() const { return grid_; }
    const std::vector<WeaponState>& weapons() const { return weapons_; }
    const std::vector<ViewProjectile>& projectiles() const { return projectiles_; }
    Vec2 ProjectilePos(const ViewProjectile& p) const;
    const std::vector<Ghost>& ghosts() const { return ghosts_; }
    const std::array<HandSlotView, kHandSize>& hand() const { return hand_; }
    // our elixir as of the tick being shown (mirrors the server exactly)
    ElixirPoints elixir() const { return elixir_; }
    // minus what pending placements will cost once the server applies them
    ElixirPoints AvailableElixir() const;
    bool CanAfford(WeaponType type) const { return AvailableElixir() >= CardCostPoints(type); }
    bool DoubleElixir() const;
    double playback() const { return clock_.playback(); }
    float SecondsLeft() const;
    float CountdownLeft() const;
    // the result, once playback has reached it
    std::optional<MsgMatchEnd> result() const;
    // the last rejected placement, cleared on read
    std::optional<PlaceError> TakeRejection();
    int checksum_mismatches() const { return checksum_mismatches_; }
    int checksums_verified() const { return checksums_verified_; }

private:
    void Reset(const MsgMatchStart& start, double now_s);
    void ApplyTick(const MsgTick& tick);
    bool ResultVisible() const;

    PlaybackClock clock_;
    bool started_ = false;
    uint32_t match_id_ = 0;
    Side side_ = Side::White;
    uint32_t start_tick_ = 0, end_tick_ = 0;
    Grid grid_ = Grid::Initial();
    std::vector<WeaponState> weapons_;
    std::vector<ViewProjectile> projectiles_;
    std::vector<Ghost> ghosts_;
    std::array<HandSlotView, kHandSize> hand_{};
    std::deque<MsgTick> buffered_;
    std::unordered_map<uint32_t, Vec2> early_hits_; // hits for projectiles not spawned in playback yet
    ElixirPoints elixir_ = kStartElixirPoints;
    std::optional<MsgMatchEnd> result_;
    std::optional<PlaceError> rejection_;
    int64_t applied_tick_ = -1;
    uint32_t next_req_id_ = 1;
    int checksum_mismatches_ = 0;
    int checksums_verified_ = 0;
};

} // namespace tt
