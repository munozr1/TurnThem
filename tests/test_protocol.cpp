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
}

TEST_CASE("server messages round-trip") {
    RoundTrip(ServerMsg{MsgQueued{}});
    const auto start = std::get<MsgMatchStart>(RoundTrip(ServerMsg{MsgMatchStart{
        5, Side::Black, 180, 10980, {WeaponType::Cannon, WeaponType::SCannon, WeaponType::SCannon, WeaponType::Cannon}}}));
    CHECK(start.your_side == Side::Black);
    CHECK(start.end_tick == 10980);
    CHECK(start.hand[1] == WeaponType::SCannon);

    const auto tick = std::get<MsgTick>(RoundTrip(ServerMsg{SampleTick()}));
    REQUIRE(tick.events.size() == 3);
    CHECK(tick.checksum == 0xDEADBEEF);
    CHECK(std::get<EvProjSpawned>(tick.events[1]).vel.x == -353.55f);
    CHECK(std::get<EvProjHit>(tick.events[2]).point.y == 39.75f);

    MsgTick empty;
    empty.tick = 1;
    CHECK(std::get<MsgTick>(RoundTrip(ServerMsg{empty})).events.empty());

    const auto result = std::get<MsgPlaceResult>(RoundTrip(ServerMsg{MsgPlaceResult{4, PlaceError::Padding, 0}}));
    CHECK(result.err == PlaceError::Padding);
    CHECK(std::get<MsgHandSlot>(RoundTrip(ServerMsg{MsgHandSlot{2, WeaponType::Cannon}})).slot == 2);
    const auto end = std::get<MsgMatchEnd>(RoundTrip(ServerMsg{MsgMatchEnd{EndReason::Forfeit, Winner::Black, 100, 160, 999}}));
    CHECK(end.winner == Winner::Black);
    CHECK(end.black == 160);
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
    CHECK(rejects_client({0x03, 1, 0, 0, 0, 0, 6, 15, 2})); // weapon type 2
    CHECK(rejects_server({0x82, 1, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0})); // side 2
    CHECK(rejects_server({0x85, 4, 0}));                     // hand slot 4
    CHECK(rejects_server({0x84, 1, 0, 0, 0, 99, 0, 0, 0, 0})); // unknown PlaceError
    CHECK(rejects_server({0x83, 1, 0, 0, 0, 2, 0, 0}));      // unknown flag bit

    MsgTick tick = SampleTick();
    tick.events = {EvProjHit{1, 13, 0, {0, 0}}}; // column 13 doesn't exist
    CHECK(rejects_server(Encode(ServerMsg{tick})));
    tick.events = {EvProjSpawned{1, 1, {NAN, 0}, {0, 0}}};
    CHECK(rejects_server(Encode(ServerMsg{tick})));
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
