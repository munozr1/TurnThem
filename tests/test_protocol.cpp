#include <math.h>
#include <random>
#include "doctest/doctest.h"
#include "tt/protocol.h"

using namespace tt;

namespace {

template <typename Msg>
Msg RoundTrip(const Msg& msg) {
    const auto bytes = Encode(msg);
    Msg out;
    REQUIRE(Decode(bytes.data(), bytes.size(), out));
    CHECK(Encode(out) == bytes); // nothing lost or reinterpreted
    // any truncation or trailing garbage is rejected
    for (size_t n = 0; n < bytes.size(); n++) {
        Msg cut;
        CHECK_FALSE(Decode(bytes.data(), n, cut));
    }
    auto longer = bytes;
    longer.push_back(0);
    Msg extra;
    CHECK_FALSE(Decode(longer.data(), longer.size(), extra));
    return out;
}

MsgTick SampleTick() {
    MsgTick tick;
    tick.tick = 123456;
    tick.has_checksum = true;
    tick.checksum = 0xDEADBEEF;
    tick.events.push_back(EvWeaponPlaced{7, WeaponType::SCannon, Side::Black, 12, 19});
    tick.events.push_back(EvProjSpawned{9, 7, {260.5f, -12.25f}, {-353.55f, 353.55f}});
    tick.events.push_back(EvProjHit{9, 0, 0, {1.5f, 39.75f}});
    tick.events.push_back(EvWeaponHit{10, 7, 65, {480.0f, 760.0f}});
    tick.events.push_back(EvWeaponHit{11, 7, 0, {480.0f, 760.0f}});
    tick.events.push_back(EvSquadPlaced{12, Side::White, 4, 12});
    InfantryState infantry;
    infantry.id = 13;
    infantry.pos = {180, 500};
    tick.events.push_back(EvBattleState{MakeHeadquarters(), {infantry}});
    return tick;
}

} // namespace

TEST_CASE("client messages round-trip") {
    CHECK(std::holds_alternative<MsgFindMatch>(RoundTrip(ClientMsg{MsgFindMatch{}})));
    CHECK(std::holds_alternative<MsgCancelFind>(RoundTrip(ClientMsg{MsgCancelFind{}})));
    CHECK(std::holds_alternative<MsgLeaveMatch>(RoundTrip(ClientMsg{MsgLeaveMatch{}})));
    const auto place = std::get<MsgPlaceRequest>(RoundTrip(ClientMsg{MsgPlaceRequest{0xA1B2C3D4, 3, 12, 19, WeaponType::SCannon}}));
    CHECK(place.req_id == 0xA1B2C3D4);
    CHECK(place.slot == 3);
    CHECK(place.col == 12);
    CHECK(place.row == 19);
    CHECK(place.expected == WeaponType::SCannon);
    CHECK_FALSE(place.setup);
    CHECK(std::get<MsgPlaceRequest>(RoundTrip(ClientMsg{MsgPlaceRequest{1, 0, 3, 15, WeaponType::Cannon, true}})).setup);
    CHECK(std::get<MsgPlaceRequest>(RoundTrip(ClientMsg{MsgPlaceRequest{1, 0, 3, 15, WeaponType::Infantry}})).expected == WeaponType::Infantry);
}

TEST_CASE("server messages round-trip") {
    RoundTrip(ServerMsg{MsgQueued{}});
    const auto start = std::get<MsgMatchStart>(RoundTrip(ServerMsg{MsgMatchStart{
        5, Side::Black, 180, 10980, {WeaponType::Cannon, WeaponType::SCannon, WeaponType::SCannon, WeaponType::Cannon}}}));
    CHECK(start.your_side == Side::Black);
    CHECK(start.end_tick == 10980);
    CHECK(start.hand[1] == WeaponType::SCannon);
    CHECK(start.setup_elixir == ElixirToPoints(6));

    const auto tick = std::get<MsgTick>(RoundTrip(ServerMsg{SampleTick()}));
    REQUIRE(tick.events.size() == 7);
    CHECK(tick.checksum == 0xDEADBEEF);
    CHECK(std::get<EvProjSpawned>(tick.events[1]).vel.x == -353.55f);
    CHECK(std::get<EvProjHit>(tick.events[2]).point.y == 39.75f);
    const auto damaged = std::get<EvWeaponHit>(tick.events[3]);
    CHECK(damaged.proj_id == 10);
    CHECK(damaged.weapon_id == 7);
    CHECK(damaged.health == 65);
    CHECK(damaged.point.x == 480.0f);
    CHECK(std::get<EvWeaponHit>(tick.events[4]).health == 0);
    CHECK(std::get<EvSquadPlaced>(tick.events[5]).squad_id == 12);
    CHECK(std::get<EvBattleState>(tick.events[6]).infantry[0].id == 13);

    MsgTick empty;
    empty.tick = 1;
    CHECK(std::get<MsgTick>(RoundTrip(ServerMsg{empty})).events.empty());

    const auto result = std::get<MsgPlaceResult>(RoundTrip(ServerMsg{MsgPlaceResult{4, PlaceError::Padding, 0}}));
    CHECK(result.err == PlaceError::Padding);
    CHECK(std::get<MsgHandSlot>(RoundTrip(ServerMsg{MsgHandSlot{2, WeaponType::Cannon}})).slot == 2);
    const auto end = std::get<MsgMatchEnd>(RoundTrip(ServerMsg{MsgMatchEnd{EndReason::Forfeit, Winner::Black, 100, 160, 999}}));
    CHECK(end.winner == Winner::Black);
    CHECK(end.black == 160);
    const auto destroyed = std::get<MsgMatchEnd>(RoundTrip(ServerMsg{MsgMatchEnd{EndReason::BaseDestroyed, Winner::White, 1000, 0, 60}}));
    CHECK(destroyed.reason == EndReason::BaseDestroyed);
}

TEST_CASE("out-of-range values are rejected") {
    auto rejects_client = [](std::vector<uint8_t> bytes) {
        ClientMsg msg;
        return !Decode(bytes.data(), bytes.size(), msg);
    };
    auto rejects_server = [](std::vector<uint8_t> bytes) {
        ServerMsg msg;
        return !Decode(bytes.data(), bytes.size(), msg);
    };
    CHECK(rejects_client({}));
    CHECK(rejects_client({0x7F}));
    CHECK(rejects_client({0x03, 1, 0, 0, 0, 0, 6, 15, 3, 0})); // unknown card
    CHECK(rejects_client({0x03, 1, 0, 0, 0, 0, 6, 15, 0, 2})); // invalid phase flag
    CHECK(rejects_server({0x82, 1, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0})); // side 2
    CHECK(rejects_server({0x85, 4, 0}));                     // hand slot 4
    CHECK(rejects_server({0x84, 1, 0, 0, 0, 99, 0, 0, 0, 0})); // unknown PlaceError
    CHECK(rejects_server({0x83, 1, 0, 0, 0, 2, 0, 0}));      // unknown flag bit

    MsgTick tick = SampleTick();
    tick.events = {EvProjHit{1, 13, 0, {0, 0}}}; // column 13 doesn't exist
    CHECK(rejects_server(Encode(ServerMsg{tick})));
    tick.events = {EvProjSpawned{1, 1, {NAN, 0}, {0, 0}}};
    CHECK(rejects_server(Encode(ServerMsg{tick})));
    tick.events = {EvWeaponHit{1, 1, kWeaponMaxHealth + 1, {0, 0}}};
    CHECK(rejects_server(Encode(ServerMsg{tick})));
    tick.events = {EvWeaponHit{1, 1, 65, {NAN, 0}}};
    CHECK(rejects_server(Encode(ServerMsg{tick})));
    tick.events = {EvWeaponHit{1, 1, 65, {0, INFINITY}}};
    CHECK(rejects_server(Encode(ServerMsg{tick})));
}

TEST_CASE("battle snapshots reject invalid health, positions, identities and excessive units") {
    auto rejects = [](EvBattleState state) {
        MsgTick tick;
        tick.events.push_back(std::move(state));
        const auto bytes = Encode(ServerMsg{tick});
        ServerMsg decoded;
        return !Decode(bytes.data(), bytes.size(), decoded);
    };
    InfantryState soldier;
    soldier.id = 10;
    soldier.pos = {120, 600};
    EvBattleState valid{MakeHeadquarters(), {soldier}};
    CHECK_FALSE(rejects(valid));
    auto bad = valid;
    bad.bases[0].health = kBaseMaxHealth + 1;
    CHECK(rejects(bad));
    bad = valid;
    bad.infantry[0].pos.x = NAN;
    CHECK(rejects(bad));
    bad.infantry[0].pos.x = kBoardW + 1;
    CHECK(rejects(bad));
    bad = valid;
    bad.infantry[0].target_kind = CombatTargetKind::Count;
    CHECK(rejects(bad));
    bad = valid;
    bad.infantry[0].health = 0;
    CHECK(rejects(bad));
    bad = valid;
    bad.infantry.push_back(soldier);
    CHECK(rejects(bad));
    bad = valid;
    bad.infantry.clear();
    for (int i = 0; i <= 2 * kMaxInfantryPerSide; i++) {
        soldier.id = (uint32_t)i + 10;
        soldier.side = i % 2 ? Side::White : Side::Black;
        bad.infantry.push_back(soldier);
    }
    CHECK(rejects(bad));
}

TEST_CASE("a full infantry field fits in one ordinary snapshot packet") {
    EvBattleState state{MakeHeadquarters(), {}};
    for (int i = 0; i < 2 * kMaxInfantryPerSide; i++) {
        InfantryState soldier;
        soldier.id = (uint32_t)i + 10;
        soldier.side = i % 2 ? Side::White : Side::Black;
        soldier.pos = {100.0f + (float)i, 500};
        state.infantry.push_back(soldier);
    }
    MsgTick tick{60, true, 123, {state}};
    CHECK(Encode(ServerMsg{tick}).size() < 1400);
    const auto decoded = std::get<MsgTick>(RoundTrip(ServerMsg{tick}));
    CHECK(std::get<EvBattleState>(decoded.events[0]).infantry.size() == 2 * kMaxInfantryPerSide);
}

TEST_CASE("random and mutated input never crashes the decoders") {
    std::mt19937_64 rng(99);
    const std::vector<std::vector<uint8_t>> seeds = {
        Encode(ServerMsg{SampleTick()}),
        Encode(ServerMsg{MsgMatchStart{1, Side::White, 1, 2, {}}}),
        Encode(ClientMsg{MsgPlaceRequest{1, 0, 6, 15, WeaponType::Cannon}}),
    };
    int decoded = 0;
    for (int i = 0; i < 200000; i++) {
        std::vector<uint8_t> bytes;
        if (i % 2) {
            bytes.resize(rng() % 48);
            for (auto& b : bytes) b = (uint8_t)rng();
        } else {
            bytes = seeds[rng() % seeds.size()];
            for (int flips = 1 + rng() % 3; flips > 0; flips--) bytes[rng() % bytes.size()] = (uint8_t)rng();
        }
        ClientMsg c;
        ServerMsg s;
        decoded += Decode(bytes.data(), bytes.size(), c);
        decoded += Decode(bytes.data(), bytes.size(), s);
    }
    CHECK(decoded > 0); // some mutations are still valid messages; the point is no crash
}
