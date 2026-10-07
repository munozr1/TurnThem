#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "doctest/doctest.h"
#include "tt/match.h"

using namespace tt;

namespace {

MatchConfig Quick(uint32_t countdown = 2, uint32_t duration = 600) {
    MatchConfig config;
    config.seed = 42;
    config.countdown_ticks = countdown;
    config.duration_ticks = duration;
    return config;
}

std::vector<Event> StepN(Match& match, int n) {
    std::vector<Event> events;
    for (int i = 0; i < n; i++) match.Step(events);
    return events;
}

void StepTo(Match& match, uint32_t tick) {
    while (match.tick() < tick) StepN(match, 1);
}

template <typename T>
std::vector<T> Only(const std::vector<Event>& events) {
    std::vector<T> out;
    for (const Event& e : events)
        if (auto* v = std::get_if<T>(&e)) out.push_back(*v);
    return out;
}

PlaceError Place(Match& match, Side side, int col, int row, uint8_t slot = 0) {
    match.SubmitPlace(side, 7, slot, (uint8_t)col, (uint8_t)row, match.hand(side)[slot % kHandSize]);
    const auto results = Only<EvPlaceResult>(StepN(match, 1));
    REQUIRE(results.size() == 1);
    return results[0].err;
}

std::string Describe(const std::vector<Event>& events) {
    std::ostringstream out;
    for (const Event& e : events) {
        std::visit([&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, EvWeaponPlaced>) out << "W" << v.weapon_id << ":" << (int)v.col << "," << (int)v.row;
            else if constexpr (std::is_same_v<T, EvProjSpawned>) out << "S" << v.proj_id << ":" << v.pos.x << "," << v.pos.y;
            else if constexpr (std::is_same_v<T, EvProjHit>) out << "H" << v.proj_id << ":" << (int)v.col << "," << (int)v.row;
            else if constexpr (std::is_same_v<T, EvPlaceResult>) out << "R" << v.req_id << ":" << (int)v.err;
            else if constexpr (std::is_same_v<T, EvHandSlot>) out << "C" << (int)v.slot << ":" << (int)v.type;
            else out << "E" << (int)v.reason << ":" << (int)v.winner;
            out << ' ';
        }, e);
    }
    return out.str();
}

} // namespace

TEST_CASE("placements during the countdown are rejected") {
    Match match(Quick(10));
    CHECK(Place(match, Side::White, 6, 15) == PlaceError::NotRunning);
}

TEST_CASE("an accepted placement spends and refills the slot") {
    Match match(Quick());
    StepTo(match, match.start_tick());
    const WeaponType card = match.hand(Side::White)[2];
    match.SubmitPlace(Side::White, 99, 2, 6, 15, card);
    const auto events = StepN(match, 1);

    const auto placed = Only<EvWeaponPlaced>(events);
    REQUIRE(placed.size() == 1);
    CHECK(placed[0].type == card);
    CHECK(placed[0].side == Side::White);
    const auto result = Only<EvPlaceResult>(events);
    REQUIRE(result.size() == 1);
    CHECK(result[0].err == PlaceError::Ok);
    CHECK(result[0].req_id == 99);
    CHECK(result[0].weapon_id == placed[0].weapon_id);
    const auto refill = Only<EvHandSlot>(events);
    REQUIRE(refill.size() == 1);
    CHECK(refill[0].to == Side::White);
    CHECK(refill[0].slot == 2);
    CHECK(refill[0].type == match.hand(Side::White)[2]);
    CHECK(match.grid().at(6, 15).occupied);
    CHECK(match.weapons().size() == 1);
}

TEST_CASE("every placement rule is enforced") {
    Match match(Quick());
    StepTo(match, match.start_tick());
    CHECK(Place(match, Side::White, 6, 5) == PlaceError::NotOwnTile);
    CHECK(Place(match, Side::Black, 6, 15) == PlaceError::NotOwnTile);
    CHECK(Place(match, Side::White, 13, 15) == PlaceError::OutOfBounds);

    match.SubmitPlace(Side::White, 1, 4, 6, 15, WeaponType::Cannon);
    CHECK(Only<EvPlaceResult>(StepN(match, 1))[0].err == PlaceError::BadSlot);

    const WeaponType wrong = match.hand(Side::White)[0] == WeaponType::Cannon ? WeaponType::SCannon : WeaponType::Cannon;
    match.SubmitPlace(Side::White, 1, 0, 6, 15, wrong);
    CHECK(Only<EvPlaceResult>(StepN(match, 1))[0].err == PlaceError::HandMismatch);

    CHECK(Place(match, Side::White, 6, 15) == PlaceError::Ok);
    StepN(match, 10);
    CHECK(Place(match, Side::White, 6, 15) == PlaceError::Occupied);
    StepN(match, 10);
    CHECK(Place(match, Side::White, 7, 16) == PlaceError::Padding);
}

TEST_CASE("placements are rate limited per player") {
    Match match(Quick());
    StepTo(match, match.start_tick());
    CHECK(Place(match, Side::White, 2, 15) == PlaceError::Ok);
    CHECK(Place(match, Side::White, 6, 15) == PlaceError::RateLimited);
    CHECK(Place(match, Side::Black, 6, 4) == PlaceError::Ok); // the other player isn't affected
    StepN(match, 5);
    CHECK(Place(match, Side::White, 6, 15) == PlaceError::Ok);
}

TEST_CASE("a cannon's shell converts the first enemy tile above it") {
    MatchConfig config = Quick();
    while (Match(config).hand(Side::White)[0] != WeaponType::Cannon) {
        config.seed++;
        REQUIRE(config.seed < 100);
    }
    Match match(config);
    StepTo(match, match.start_tick());
    match.SubmitPlace(Side::White, 1, 0, 6, 12, WeaponType::Cannon);
    const uint32_t placed_tick = match.tick();
    std::vector<Event> events;
    match.Step(events);

    const auto spawns = Only<EvProjSpawned>(events);
    REQUIRE(spawns.size() == 1); // fires on its placement tick
    for (int i = 0; i < 8; i++) match.Step(events);
    const auto hits = Only<EvProjHit>(events);
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].proj_id == spawns[0].proj_id);
    CHECK((hits[0].col == 6 && hits[0].row == 9));
    CHECK(hits[0].point.y == doctest::Approx(400));
    CHECK(match.grid().at(6, 9).side == Side::White);
    CHECK(match.tick() == placed_tick + 9);
}

TEST_CASE("both players' weapons convert tiles, whatever cards are dealt") {
    for (uint64_t seed = 1; seed <= 8; seed++) {
        CAPTURE(seed);
        MatchConfig config = Quick(2, 1200);
        config.seed = seed;
        Match match(config);
        std::vector<Event> events; // the whole match
        while (!match.ended()) {
            if (match.tick() == match.start_tick())
                match.SubmitPlace(Side::White, 1, 0, 6, 12, match.hand(Side::White)[0]);
            if (match.tick() == match.start_tick() + 10)
                match.SubmitPlace(Side::Black, 2, 1, 6, 7, match.hand(Side::Black)[1]);
            match.Step(events);
        }
        REQUIRE(Only<EvWeaponPlaced>(events).size() == 2);

        // who fired each projectile: proj -> weapon -> side
        std::map<uint32_t, Side> weapon_side, proj_side;
        for (const auto& placed : Only<EvWeaponPlaced>(events)) weapon_side[placed.weapon_id] = placed.side;
        for (const auto& spawn : Only<EvProjSpawned>(events)) proj_side[spawn.proj_id] = weapon_side.at(spawn.weapon_id);
        int hits[2] = {0, 0};
        for (const auto& hit : Only<EvProjHit>(events)) hits[(int)proj_side.at(hit.proj_id)]++;
        CHECK(hits[0] > 0);
        CHECK(hits[1] > 0);
        CHECK(match.grid().Count(Side::White) + match.grid().Count(Side::Black) == 260);
    }
}

TEST_CASE("the timer ends the match and the bigger colour wins") {
    Match even(Quick(2, 30));
    auto ended = Only<EvMatchEnded>(StepN(even, 40));
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].reason == EndReason::Timer);
    CHECK(ended[0].winner == Winner::Draw);
    CHECK(ended[0].white == 130);
    CHECK(even.ended());
    CHECK(StepN(even, 5).empty()); // nothing happens after the end

    Match white_wins(Quick(2, 60));
    StepTo(white_wins, white_wins.start_tick());
    REQUIRE(Place(white_wins, Side::White, 6, 12) == PlaceError::Ok);
    ended = Only<EvMatchEnded>(StepN(white_wins, 100));
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].winner == Winner::White);
    CHECK(ended[0].white > ended[0].black);
}

TEST_CASE("quitting cancels during the countdown and forfeits afterwards") {
    Match early(Quick(30));
    StepN(early, 5);
    early.Forfeit(Side::White);
    auto ended = Only<EvMatchEnded>(StepN(early, 1));
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].reason == EndReason::Cancelled);

    Match late(Quick(2));
    StepN(late, 20);
    late.Forfeit(Side::Black);
    ended = Only<EvMatchEnded>(StepN(late, 1));
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].reason == EndReason::Forfeit);
    CHECK(ended[0].winner == Winner::White);
}

TEST_CASE("same seed and same inputs give the same match") {
    auto play = [](uint64_t seed) {
        MatchConfig config = Quick(2, 900);
        config.seed = seed;
        Match match(config);
        std::vector<Event> events;
        for (int t = 0; t < 1000; t++) {
            if (t == 5) match.SubmitPlace(Side::White, 1, 0, 3, 13, match.hand(Side::White)[0]);
            if (t == 9) match.SubmitPlace(Side::Black, 2, 1, 9, 6, match.hand(Side::Black)[1]);
            if (t == 200) match.SubmitPlace(Side::White, 3, 2, 9, 15, match.hand(Side::White)[2]);
            match.Step(events);
        }
        return Describe(events) + std::to_string(match.grid().Checksum());
    };
    CHECK(play(7) == play(7));
}
