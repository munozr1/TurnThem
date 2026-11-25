#include <random>
#include "doctest/doctest.h"
#include "tt/bot.h"
#include "tt/loopback_link.h"
#include "tt/match_view.h"

using namespace tt;

namespace {

struct Outcome {
    MatchView view;
    const MatchHost* host = nullptr;
    int placed = 0;
    int rejected = 0;
    bool saw_countdown = false;
    bool saw_running = false;
};

// plays a whole practice match through the loopback link, placing weapons
// for the human side with the bot policy, the way the client UI would
void Play(Outcome& out, LoopbackLink& link, double fps, double until, double leave_at = -1) {
    std::mt19937_64 rng(3);
    link.Send(MsgFindMatch{});
    double next_place = 0;
    for (double now = 0; now < until; now += 1.0 / fps) {
        link.Pump(now);
        ServerMsg msg;
        while (link.Receive(msg)) out.view.OnMessage(msg, now);
        out.view.Update(now);
        if (auto rejection = out.view.TakeRejection()) out.rejected++;

        const auto phase = out.view.phase();
        out.saw_countdown |= phase == MatchView::Phase::Countdown;
        out.saw_running |= phase == MatchView::Phase::Running;
        if (phase == MatchView::Phase::Running && now >= next_place) {
            next_place = now + 0.7;
            const auto choice = ChooseBotPlacement(out.view.grid(), out.view.side(), rng, [&](int c, int r) {
                return out.view.CheckLocal(c, r) == PlaceError::Ok;
            });
            for (uint8_t slot = 0; choice && slot < kHandSize; slot++) {
                if (auto request = out.view.RequestPlace(slot, choice->col, choice->row)) {
                    link.Send(*request);
                    out.placed++;
                    break;
                }
            }
        }
        if (leave_at >= 0 && now >= leave_at) {
            link.Send(MsgLeaveMatch{});
            leave_at = -1;
        }
        if (out.view.result()) break;
    }
    out.host = link.host();
}

LoopbackOptions Options(Side human, double lag, double jitter) {
    LoopbackOptions options;
    options.human = human;
    options.config.countdown_ticks = 60;
    options.config.duration_ticks = 20 * kTickRate;
    options.lag_s = lag;
    options.jitter_s = jitter;
    options.seed = 11;
    return options;
}

void CheckConverged(const Outcome& out) {
    REQUIRE(out.host);
    REQUIRE(out.view.result());
    const auto result = *out.view.result();
    CHECK((result.reason == EndReason::Timer || result.reason == EndReason::BaseDestroyed));
    CHECK(out.saw_countdown);
    CHECK(out.saw_running);
    CHECK(out.placed > 0);
    // the client's played-back board is the server's board, tile for tile
    CHECK(out.view.checksum_mismatches() == 0);
    CHECK(out.view.checksums_verified() > 0);
    CHECK(out.view.grid().Checksum() == out.host->match().grid().Checksum());
    CHECK(BattleChecksum(out.view.grid(), out.view.weapons(), out.view.bases(), out.view.infantry()) == out.host->match().checksum());
    CHECK(result.white == out.view.bases()[0].health);
    CHECK(result.black == out.view.bases()[1].health);
    REQUIRE(out.view.weapons().size() == out.host->match().weapons().size());
    for (size_t i = 0; i < out.view.weapons().size(); i++) {
        const WeaponState& shown = out.view.weapons()[i];
        const WeaponState& actual = out.host->match().weapons()[i];
        CHECK(shown.id == actual.id);
        CHECK(shown.side == actual.side);
        CHECK(shown.type == actual.type);
        CHECK(shown.health == actual.health);
        CHECK(shown.col == actual.col);
        CHECK(shown.row == actual.row);
    }
    CHECK(out.view.elixir() == out.host->match().elixir(out.view.side())); // exact mirror
    CHECK(out.view.ghosts().empty());
    for (const auto& slot : out.view.hand()) CHECK_FALSE(slot.pending);
}

} // namespace

TEST_CASE("a practice match converges with no network delay") {
    LoopbackLink link(Options(Side::White, 0, 0));
    Outcome out;
    Play(out, link, 60, 40);
    CheckConverged(out);
}

TEST_CASE("a practice match converges with lag and jitter, as black, at an odd frame rate") {
    LoopbackLink link(Options(Side::Black, 0.08, 0.04));
    Outcome out;
    Play(out, link, 47, 40);
    CheckConverged(out);
}

TEST_CASE("leaving mid-match forfeits and shows the result straight away") {
    LoopbackLink link(Options(Side::White, 0.05, 0));
    Outcome out;
    Play(out, link, 60, 40, 5.0);
    REQUIRE(out.view.result());
    // let the final TICK play back too, then the elixir mirror must still be exact
    for (double now = 40; now < 41; now += 1.0 / 60) {
        link.Pump(now);
        ServerMsg msg;
        while (link.Receive(msg)) out.view.OnMessage(msg, now);
        out.view.Update(now);
    }
    CHECK(out.view.elixir() == link.host()->match().elixir(out.view.side()));
    CHECK(out.view.result()->reason == EndReason::Forfeit);
    CHECK(out.view.result()->winner == Winner::Black);
    CHECK(out.view.phase() == MatchView::Phase::Ended);
}

TEST_CASE("projectiles are drawn moving and stop at their hit point") {
    LoopbackLink link(Options(Side::White, 0.05, 0.02));
    Outcome out;
    std::mt19937_64 rng(5);
    link.Send(MsgFindMatch{});
    bool placed = false, saw_moving = false;
    for (double now = 0; now < 8; now += 1.0 / 60) {
        link.Pump(now);
        ServerMsg msg;
        while (link.Receive(msg)) out.view.OnMessage(msg, now);
        out.view.Update(now);
        if (!placed && out.view.phase() == MatchView::Phase::Running) {
            if (auto request = out.view.RequestPlace(0, 6, 12)) {
                link.Send(*request);
                placed = true;
            }
        }
        for (const auto& p : out.view.projectiles()) {
            const Vec2 pos = out.view.ProjectilePos(p);
            if (p.side == Side::White) {
                saw_moving |= pos.y < p.spawn.y;
                if (p.hit_known) CHECK(pos.y >= p.hit_point.y - 0.01f); // never drawn past the tile it hits
            }
        }
    }
    CHECK(placed);
    CHECK(saw_moving);
    CHECK(out.view.grid().at(6, 9).side == Side::Black);
}

TEST_CASE("a placement result that arrives after its weapon was played back leaves no ghost") {
    MatchView view(0.0); // no delay: everything plays back as soon as it arrives
    const Hand hand = {WeaponType::Cannon, WeaponType::Cannon, WeaponType::SCannon, WeaponType::SCannon};
    double now = 0;
    view.OnMessage(MsgMatchStart{1, Side::White, 0, 6000, hand}, now);
    auto tick = [&](uint32_t t, std::vector<TickEvent> events = {}) {
        MsgTick msg;
        msg.tick = t;
        msg.events = std::move(events);
        view.OnMessage(msg, now);
        view.Update(now);
        now += 1.0 / kTickRate;
    };
    for (uint32_t t = 0; t <= 20; t++) tick(t);
    const auto request = view.RequestPlace(0, 6, 15);
    REQUIRE(request);
    REQUIRE(view.ghosts().size() == 1);

    // the TICK with the weapon arrives and is played back before PLACE_RESULT (a stall split them)
    tick(21, {EvWeaponPlaced{1, WeaponType::Cannon, Side::White, 6, 15}});
    CHECK(view.weapons().size() == 1);
    CHECK(view.ghosts().empty());                       // settled by its weapon, before the result
    CHECK(view.AvailableElixir() == view.elixir());     // so its cost isn't counted twice
    view.OnMessage(MsgPlaceResult{request->req_id, PlaceError::Ok, 1}, now);
    view.OnMessage(MsgHandSlot{0, WeaponType::SCannon}, now);
    for (uint32_t t = 22; t < 40; t++) tick(t);
    CHECK(view.ghosts().empty());
    CHECK_FALSE(view.hand()[0].pending);
}

TEST_CASE("the countdown starts at 3, not 4, while playback fills its buffer") {
    LoopbackOptions options = Options(Side::White, 0.05, 0);
    options.config.countdown_ticks = 180;
    LoopbackLink link(options);
    MatchView view(6);
    link.Send(MsgFindMatch{});
    float highest = 0;
    for (double now = 0; now < 1; now += 1.0 / 60) {
        link.Pump(now);
        ServerMsg msg;
        while (link.Receive(msg)) view.OnMessage(msg, now);
        view.Update(now);
        if (view.phase() == MatchView::Phase::Countdown) highest = std::max(highest, view.CountdownLeft());
    }
    CHECK(highest > 2.9f);
    CHECK(highest <= 3.0f);
}

TEST_CASE("weapon damage plays back in order and destruction leaves already fired shells alive") {
    MatchView view(0.0);
    const Hand hand = {WeaponType::Cannon, WeaponType::Cannon, WeaponType::SCannon, WeaponType::SCannon};
    view.OnMessage(MsgMatchStart{1, Side::White, 0, 6000, hand}, 0);
    auto tick = [&](uint32_t t, std::vector<TickEvent> events) {
        MsgTick msg;
        msg.tick = t;
        msg.events = std::move(events);
        const double now = (double)t / kTickRate;
        view.OnMessage(msg, now);
        view.Update(now + 0.01);
    };
    tick(0, {EvWeaponPlaced{1, WeaponType::Cannon, Side::White, 6, 12},
             EvWeaponPlaced{2, WeaponType::SCannon, Side::Black, 6, 7},
             EvProjSpawned{1, 1, {260, 500}, {0, -800}},
             EvProjSpawned{10, 2, {260, 300}, {0, 500}}});
    REQUIRE(view.weapons().size() == 2);
    CHECK(view.weapons()[1].health == kWeaponMaxHealth);
    CHECK(view.grid().at(6, 7).occupied);
    CHECK(view.grid().at(5, 7).padding);

    tick(1, {EvWeaponHit{1, 2, 65, {260, 320}}, EvProjSpawned{2, 1, {260, 500}, {0, -800}}});
    REQUIRE(view.weapons().size() == 2);
    CHECK(view.weapons()[1].health == 65);
    CHECK(view.grid().at(6, 7).side == Side::Black);
    REQUIRE(view.projectiles().size() == 2);
    CHECK(view.projectiles()[0].id == 10);
    CHECK(view.projectiles()[1].id == 2);

    tick(2, {EvWeaponHit{2, 2, 30, {260, 320}}, EvProjSpawned{3, 1, {260, 500}, {0, -800}}});
    REQUIRE(view.weapons().size() == 2);
    CHECK(view.weapons()[1].health == 30);
    tick(3, {EvWeaponHit{3, 2, 0, {260, 320}}});
    REQUIRE(view.weapons().size() == 1);
    CHECK(view.weapons()[0].id == 1);
    CHECK_FALSE(view.grid().at(6, 7).occupied);
    CHECK_FALSE(view.grid().at(5, 7).padding);
    CHECK(view.grid().at(6, 7).side == Side::Black);
    REQUIRE(view.projectiles().size() == 1);
    CHECK(view.projectiles()[0].id == 10);
    CHECK(view.projectiles()[0].side == Side::Black);

    // The destroyed gun's shell still resolves its hit without changing the field.
    tick(4, {EvProjHit{10, 6, 10, {260, 400}}});
    CHECK(view.projectiles().empty());
    CHECK(view.grid().at(6, 10).side == Side::White);
}

TEST_CASE("buffered weapon hits clamp shells without applying health early") {
    bool update_before_hit = false;
    SUBCASE("the hit arrives before the spawn is played back") {}
    SUBCASE("the projectile is already visible when the hit arrives") { update_before_hit = true; }

    MatchView view(0.25);
    const Hand hand = {WeaponType::Cannon, WeaponType::Cannon, WeaponType::SCannon, WeaponType::SCannon};
    view.OnMessage(MsgMatchStart{1, Side::White, 0, 6000, hand}, 0);
    for (uint32_t t = 0; t <= 8; t++) {
        MsgTick msg;
        msg.tick = t;
        if (t == 0) {
            msg.events = {EvWeaponPlaced{1, WeaponType::Cannon, Side::White, 6, 12},
                          EvWeaponPlaced{2, WeaponType::SCannon, Side::Black, 6, 9},
                          EvProjSpawned{1, 1, {260, 500}, {0, -800}}};
        } else if (t == 8) {
            msg.events = {EvWeaponHit{1, 2, 65, {260, 400}}};
        }
        const double now = (double)t / kTickRate;
        view.OnMessage(msg, now);
        if (update_before_hit || t == 8) view.Update(now);
    }

    REQUIRE(view.weapons().size() == 2);
    CHECK(view.weapons()[1].health == kWeaponMaxHealth);
    CHECK(view.grid().at(6, 9).side == Side::Black);
    REQUIRE(view.projectiles().size() == 1);
    CHECK(view.projectiles()[0].hit_known);
    CHECK(view.ProjectilePos(view.projectiles()[0]).y == doctest::Approx(400));

    view.Update(9.0 / kTickRate);
    REQUIRE(view.weapons().size() == 2);
    CHECK(view.weapons()[1].health == 65);
    CHECK(view.projectiles().empty());
    CHECK(view.grid().at(6, 9).side == Side::Black);
}

TEST_CASE("setup reserves its own allowance and freezes weapons until combat") {
    MatchView view(0);
    const Hand hand{WeaponType::Cannon, WeaponType::SCannon, WeaponType::Infantry, WeaponType::Cannon};
    view.OnMessage(MsgMatchStart{1, Side::White, 60, 6000, hand}, 0);
    auto tick = [&](uint32_t t, std::vector<TickEvent> events = {}) {
        view.OnMessage(MsgTick{t, false, 0, std::move(events)}, (double)t / kTickRate);
        view.Update((double)t / kTickRate + 0.001);
    };
    tick(0);
    REQUIRE(view.phase() == MatchView::Phase::Setup);
    CHECK(view.CheckPlace(2, 3, 15) == PlaceError::SetupArtilleryOnly);
    auto rejected = view.RequestPlace(0, 3, 15);
    REQUIRE(rejected);
    CHECK(rejected->setup);
    CHECK(view.AvailableElixir() == ElixirToPoints(3));
    CHECK(view.elixir() == kStartElixirPoints);
    view.OnMessage(MsgPlaceResult{rejected->req_id, PlaceError::RateLimited, 0}, 0);
    CHECK(view.AvailableElixir() == ElixirToPoints(6));
    CHECK_FALSE(view.hand()[0].pending);

    const auto request = view.RequestPlace(0, 3, 15);
    REQUIRE(request);
    tick(1, {EvWeaponPlaced{1, WeaponType::Cannon, Side::White, 3, 15}});
    REQUIRE(view.weapons().size() == 1);
    CHECK(view.weapons()[0].age == 0);
    CHECK(view.setup_elixir() == ElixirToPoints(3));
    CHECK(view.elixir() == kStartElixirPoints);
    for (uint32_t t = 2; t < 60; t++) tick(t);
    const auto delayed_setup = view.RequestPlace(3, 9, 15);
    REQUIRE(delayed_setup);
    CHECK(delayed_setup->setup);
    CHECK(view.AvailableElixir() == 0);
    tick(60);
    CHECK(view.phase() == MatchView::Phase::Running);
    CHECK(view.weapons()[0].age == 1);
    CHECK(view.elixir() == kStartElixirPoints + 1);
    CHECK(view.AvailableElixir() == view.elixir());
    view.OnMessage(MsgPlaceResult{delayed_setup->req_id, PlaceError::NotRunning, 0}, 1.01);
    CHECK_FALSE(view.hand()[3].pending);
    CHECK(view.AvailableElixir() == kStartElixirPoints + 1);
}

TEST_CASE("a three-soldier card settles once across early and late acknowledgements") {
    bool early_ack = false;
    SUBCASE("acknowledgement before playback") { early_ack = true; }
    SUBCASE("acknowledgement after playback") {}
    MatchView view(0);
    const Hand hand{WeaponType::Infantry, WeaponType::Cannon, WeaponType::Infantry, WeaponType::SCannon};
    view.OnMessage(MsgMatchStart{1, Side::White, 0, 6000, hand}, 0);
    view.OnMessage(MsgTick{0, false, 0, {}}, 0);
    view.Update(0);
    const auto request = view.RequestPlace(0, 3, 15);
    REQUIRE(request);
    CHECK_FALSE(request->setup);
    CHECK(view.AvailableElixir() == kStartElixirPoints + 1 - CardCostPoints(WeaponType::Infantry));
    if (early_ack) view.OnMessage(MsgPlaceResult{request->req_id, PlaceError::Ok, 1}, 0.01);
    EvBattleState state{MakeHeadquarters(), {}};
    const auto positions = SquadPositions(Side::White, 3, 15);
    for (int i = 0; i < kSquadSize; i++) {
        InfantryState soldier;
        soldier.id = (uint32_t)i + 2;
        soldier.pos = positions[i];
        state.infantry.push_back(soldier);
    }
    view.OnMessage(MsgTick{1, false, 0, {EvSquadPlaced{1, Side::White, 3, 15}, state}}, 1.0 / kTickRate);
    view.Update(1.0 / kTickRate + 0.001);
    REQUIRE(view.infantry().size() == 3);
    CHECK(view.ghosts().empty());
    CHECK(view.elixir() == kStartElixirPoints + 2 - CardCostPoints(WeaponType::Infantry));
    CHECK(view.AvailableElixir() == view.elixir());
    view.OnMessage(MsgPlaceResult{request->req_id, PlaceError::Ok, 1}, 0.04);
    view.OnMessage(MsgHandSlot{0, WeaponType::Cannon}, 0.04);
    CHECK(view.ghosts().empty());
    CHECK_FALSE(view.hand()[0].pending);

    // A casualty removes only that individual from the authoritative squad.
    state.infantry.erase(state.infantry.begin());
    view.OnMessage(MsgTick{2, false, 0, {state}}, 2.0 / kTickRate);
    view.Update(2.0 / kTickRate + 0.001);
    CHECK(view.infantry().size() == 2);
    CHECK(view.elixir() == kStartElixirPoints + 3 - CardCostPoints(WeaponType::Infantry));
}

TEST_CASE("a headquarters result waits for the lethal damage tick to play back") {
    MatchView view(6);
    view.OnMessage(MsgMatchStart{1, Side::White, 0, 6000, {}}, 0);
    for (uint32_t t = 0; t <= 10; t++) {
        EvBattleState state{MakeHeadquarters(), {}};
        if (t == 10) state.bases[1].health = 0;
        std::vector<TickEvent> events;
        if (t == 0) events.push_back(EvWeaponPlaced{3, WeaponType::Cannon, Side::White, 3, 15});
        events.push_back(state);
        view.OnMessage(MsgTick{t, false, 0, std::move(events)}, (double)t / kTickRate);
        view.Update((double)t / kTickRate);
    }
    view.OnMessage(MsgMatchEnd{EndReason::BaseDestroyed, Winner::White, kBaseMaxHealth, 0, 10}, 10.0 / kTickRate);
    CHECK_FALSE(view.result());
    CHECK(view.bases()[1].health == kBaseMaxHealth);
    view.Update(1);
    REQUIRE(view.result());
    CHECK(view.bases()[1].health == 0);
    CHECK(view.phase() == MatchView::Phase::Ended);
    REQUIRE(view.weapons().size() == 1);
    CHECK(view.weapons()[0].age == 10);
    CHECK_FALSE(view.RequestPlace(0, 3, 15));
}

TEST_CASE("battle checksums detect infantry drift even when the grid is identical") {
    MatchView view(0);
    view.OnMessage(MsgMatchStart{1, Side::White, 0, 6000, {}}, 0);
    InfantryState soldier;
    soldier.id = 5;
    soldier.pos = {100, 500};
    EvBattleState state{MakeHeadquarters(), {soldier}};
    const uint32_t checksum = BattleChecksum(Grid::Initial(), {}, state.bases, state.infantry);
    view.OnMessage(MsgTick{0, true, checksum, {state}}, 0);
    view.Update(0);
    CHECK(view.checksums_verified() == 1);
    state.infantry[0].health--;
    view.OnMessage(MsgTick{1, true, checksum, {state}}, 1.0 / kTickRate);
    view.Update(1.0 / kTickRate + 0.001);
    CHECK(view.checksum_mismatches() == 1);
}
