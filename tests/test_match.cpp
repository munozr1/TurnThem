#include <algorithm>
#include <set>
#include <vector>
#include "doctest/doctest.h"
#include "tt/match.h"

using namespace tt;

namespace {
MatchConfig Quick(uint32_t setup = 2, uint32_t duration = 600) {
    MatchConfig config;
    config.seed = 42;
    config.countdown_ticks = setup;
    config.duration_ticks = duration;
    return config;
}
std::vector<Event> StepN(Match& match, int n) {
    std::vector<Event> events;
    for (int i = 0; i < n; i++) match.Step(events);
    return events;
}
void StepTo(Match& match, uint32_t tick) {
    while (match.tick() < tick && !match.ended()) StepN(match, 1);
}
template <typename T> std::vector<T> Only(const std::vector<Event>& events) {
    std::vector<T> out;
    for (const Event& e : events) if (auto* v = std::get_if<T>(&e)) out.push_back(*v);
    return out;
}
PlaceError Place(Match& match, Side side, int col, int row, uint8_t slot = 0) {
    match.SubmitPlace(side, 7, slot, (uint8_t)col, (uint8_t)row, match.hand(side)[slot % kHandSize]);
    const auto results = Only<EvPlaceResult>(StepN(match, 1));
    REQUIRE(results.size() == 1);
    return results[0].err;
}
}

TEST_CASE("setup allows two artillery placements without firing or spending battle supplies") {
    Match match(Quick(60, 6000));
    CHECK(match.phase() == MatchPhase::Setup);
    CHECK(match.setup_elixir(Side::White) == ElixirToPoints(6));
    REQUIRE(Place(match, Side::White, 3, 15) == PlaceError::Ok);
    StepTo(match, 10);
    REQUIRE(Place(match, Side::White, 9, 15, 1) == PlaceError::Ok);
    StepTo(match, 20);
    CHECK(Place(match, Side::White, 6, 12) == PlaceError::NotEnoughElixir);
    CHECK(match.setup_elixir(Side::White) == 0);
    CHECK(match.elixir(Side::White) == kStartElixirPoints);
    StepTo(match, match.start_tick());
    REQUIRE(match.weapons().size() == 2);
    for (const auto& weapon : match.weapons()) CHECK(weapon.age == 0);
    CHECK(match.projectiles().empty());
    for (const auto& base : match.bases()) CHECK(base.health == kBaseMaxHealth);
    CHECK(match.phase() == MatchPhase::Combat);
    const auto events = StepN(match, 1);
    CHECK_FALSE(Only<EvProjSpawned>(events).empty());
    CHECK(match.elixir(Side::White) == kStartElixirPoints + 1);
    for (const auto& weapon : match.weapons()) CHECK(weapon.age == 1);
}

TEST_CASE("infantry is reserved for combat and rejected setup costs nothing") {
    Match match(Quick(30));
    const Hand hand = match.hand(Side::White);
    REQUIRE(hand[2] == WeaponType::Infantry);
    CHECK(Place(match, Side::White, 6, 12, 2) == PlaceError::SetupArtilleryOnly);
    CHECK(match.hand(Side::White) == hand);
    CHECK(match.infantry().empty());
    CHECK(match.setup_elixir(Side::White) == ElixirToPoints(6));
    CHECK(match.elixir(Side::White) == kStartElixirPoints);
}

TEST_CASE("stale setup deployment cannot spend combat supplies") {
    Match match(Quick(1));
    StepTo(match, match.start_tick());
    match.SubmitPlace(Side::White, 99, 0, 6, 12, WeaponType::Cannon, true);
    const auto events = StepN(match, 1);
    const auto results = Only<EvPlaceResult>(events);
    REQUIRE(results.size() == 1);
    CHECK(results[0].err == PlaceError::NotRunning);
    CHECK(match.weapons().empty());
    CHECK(match.elixir(Side::White) == kStartElixirPoints + 2); // final-minute bonus in short match
    CHECK(match.setup_elixir(Side::White) == ElixirToPoints(6));
}

TEST_CASE("an artillery placement spends once and refills its slot") {
    Match match(Quick());
    StepTo(match, match.start_tick());
    match.SubmitPlace(Side::White, 99, 0, 6, 15, WeaponType::Cannon);
    const auto events = StepN(match, 1);
    const auto placed = Only<EvWeaponPlaced>(events);
    const auto result = Only<EvPlaceResult>(events);
    const auto refill = Only<EvHandSlot>(events);
    REQUIRE(placed.size() == 1);
    REQUIRE(result.size() == 1);
    REQUIRE(refill.size() == 1);
    CHECK(placed[0].type == WeaponType::Cannon);
    CHECK(result[0].req_id == 99);
    CHECK(result[0].err == PlaceError::Ok);
    CHECK(result[0].weapon_id == placed[0].weapon_id);
    CHECK(refill[0].slot == 0);
    CHECK(refill[0].type == match.hand(Side::White)[0]);
    CHECK(match.grid().at(6, 15).occupied);
    CHECK(match.elixir(Side::White) == kStartElixirPoints + 2 - CardCostPoints(WeaponType::Cannon));
}

TEST_CASE("deployment enforces fixed zones, occupied cells, spacing, slots and hand state") {
    Match match(Quick());
    StepTo(match, match.start_tick());
    CHECK(Place(match, Side::White, 6, 5) == PlaceError::NotOwnTile);
    CHECK(Place(match, Side::Black, 6, 15) == PlaceError::NotOwnTile);
    CHECK(Place(match, Side::White, 13, 15) == PlaceError::OutOfBounds);
    match.SubmitPlace(Side::White, 1, 4, 6, 15, WeaponType::Cannon);
    CHECK(Only<EvPlaceResult>(StepN(match, 1))[0].err == PlaceError::BadSlot);
    match.SubmitPlace(Side::White, 1, 0, 6, 15, WeaponType::SCannon);
    CHECK(Only<EvPlaceResult>(StepN(match, 1))[0].err == PlaceError::HandMismatch);
    CHECK(Place(match, Side::White, 6, 15) == PlaceError::Ok);
    StepN(match, 10);
    CHECK(Place(match, Side::White, 6, 15, 1) == PlaceError::Occupied);
    StepN(match, 10);
    CHECK(Place(match, Side::White, 7, 16, 1) == PlaceError::Padding);
}

TEST_CASE("placements are rate limited independently for each player") {
    auto config = Quick();
    config.start_elixir = kMaxElixirPoints;
    Match match(config);
    StepTo(match, match.start_tick());
    CHECK(Place(match, Side::White, 2, 15) == PlaceError::Ok);
    CHECK(Place(match, Side::White, 6, 15, 1) == PlaceError::RateLimited);
    CHECK(Place(match, Side::Black, 6, 4) == PlaceError::Ok);
    StepN(match, 5);
    CHECK(Place(match, Side::White, 6, 15, 1) == PlaceError::Ok);
}

TEST_CASE("one squad card deploys three distinct soldiers and spends only once") {
    Match match(Quick(0, 6000));
    const Grid before = match.grid();
    match.SubmitPlace(Side::White, 55, 2, 6, 12, WeaponType::Infantry);
    const auto events = StepN(match, 1);
    REQUIRE(Only<EvPlaceResult>(events).size() == 1);
    CHECK(Only<EvPlaceResult>(events)[0].err == PlaceError::Ok);
    REQUIRE(Only<EvSquadPlaced>(events).size() == 1);
    CHECK(Only<EvPlaceResult>(events)[0].weapon_id == Only<EvSquadPlaced>(events)[0].squad_id);
    CHECK(Only<EvHandSlot>(events).size() == 1);
    CHECK(Only<EvWeaponPlaced>(events).empty());
    REQUIRE(match.infantry().size() == kSquadSize);
    std::set<uint32_t> ids;
    for (const auto& soldier : match.infantry()) {
        ids.insert(soldier.id);
        CHECK(soldier.side == Side::White);
        CHECK(soldier.health == kInfantryMaxHealth);
    }
    CHECK(ids.size() == kSquadSize);
    CHECK(match.elixir(Side::White) == kStartElixirPoints + 1 - CardCostPoints(WeaponType::Infantry));
    CHECK(match.grid().Checksum() == before.Checksum());
}

TEST_CASE("failed squad placement leaves no partial soldiers or resource charge") {
    Match match(Quick(0, 6000));
    const auto before = match.hand(Side::White);
    match.SubmitPlace(Side::White, 55, 2, 6, 18, WeaponType::Infantry); // headquarters
    const auto events = StepN(match, 1);
    REQUIRE(Only<EvPlaceResult>(events).size() == 1);
    CHECK(Only<EvPlaceResult>(events)[0].err != PlaceError::Ok);
    CHECK(Only<EvSquadPlaced>(events).empty());
    CHECK(Only<EvHandSlot>(events).empty());
    CHECK(match.infantry().empty());
    CHECK(match.hand(Side::White) == before);
    CHECK(match.elixir(Side::White) == kStartElixirPoints + 1);
}

TEST_CASE("artillery shells cross empty enemy ground and destroy the enemy headquarters") {
    for (Side side : {Side::White, Side::Black}) {
        Match match(Quick(0, 3600));
        REQUIRE(Place(match, side, 6, side == Side::White ? 10 : 9) == PlaceError::Ok);
        const auto events = StepN(match, 3601);
        const auto end = Only<EvMatchEnded>(events);
        REQUIRE(end.size() == 1);
        CHECK(end[0].reason == EndReason::BaseDestroyed);
        CHECK(end[0].winner == (side == Side::White ? Winner::White : Winner::Black));
        CHECK(match.bases()[(int)Opponent(side)].health == 0);
        CHECK(match.bases()[(int)side].health == kBaseMaxHealth);
        CHECK(match.grid().Count(Side::White) == kCols * kRows / 2);
        CHECK(match.grid().Count(Side::Black) == kCols * kRows / 2);
        CHECK(StepN(match, 5).empty());
    }
}

TEST_CASE("surviving headquarters draw at timeout even when their health differs") {
    Match match(Quick(0, 300));
    REQUIRE(Place(match, Side::White, 6, 10) == PlaceError::Ok);
    const auto end = Only<EvMatchEnded>(StepN(match, 400));
    REQUIRE(end.size() == 1);
    CHECK(end[0].reason == EndReason::Timer);
    CHECK(end[0].winner == Winner::Draw);
    CHECK(end[0].white == kBaseMaxHealth);
    CHECK(end[0].black > 0);
    CHECK(end[0].black < end[0].white);
}

TEST_CASE("infantry advances to the enemy base, stops in range and fires") {
    for (Side side : {Side::White, Side::Black}) {
        Match match(Quick(0, 6000));
        REQUIRE(Place(match, side, 6, side == Side::White ? 12 : 7, 2) == PlaceError::Ok);
        const float initial_y = match.infantry()[0].pos.y;
        StepN(match, 60);
        REQUIRE(match.infantry().size() == kSquadSize);
        CHECK((side == Side::White ? match.infantry()[0].pos.y < initial_y : match.infantry()[0].pos.y > initial_y));
        for (int t = 0; t < 1500 && match.bases()[(int)Opponent(side)].health == kBaseMaxHealth; t++) StepN(match, 1);
        REQUIRE(match.bases()[(int)Opponent(side)].health < kBaseMaxHealth);
        REQUIRE(match.infantry().size() == kSquadSize);
        const auto soldier = match.infantry()[0];
        CHECK(soldier.phase == InfantryPhase::Attacking);
        CHECK(soldier.target_kind == CombatTargetKind::Headquarters);
        StepN(match, 5);
        CHECK(match.infantry()[0].pos.x == doctest::Approx(soldier.pos.x));
        CHECK(match.infantry()[0].pos.y == doctest::Approx(soldier.pos.y));
        CHECK(match.bases()[(int)side].health == kBaseMaxHealth);
    }
}

TEST_CASE("quitting cancels setup and forfeits combat") {
    Match early(Quick(30));
    REQUIRE(Place(early, Side::White, 3, 15) == PlaceError::Ok);
    early.Forfeit(Side::White);
    auto ended = Only<EvMatchEnded>(StepN(early, 1));
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].reason == EndReason::Cancelled);
    CHECK(ended[0].winner == Winner::Draw);
    Match late(Quick(2));
    StepN(late, 20);
    late.Forfeit(Side::Black);
    ended = Only<EvMatchEnded>(StepN(late, 1));
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].reason == EndReason::Forfeit);
    CHECK(ended[0].winner == Winner::White);
}

TEST_CASE("same seed and deployment inputs produce identical complete battle state") {
    Match a(Quick(20, 1200)), b(Quick(20, 1200));
    for (int tick = 0; tick < 1300; tick++) {
        for (Match* match : {&a, &b}) {
            if (tick == 1) match->SubmitPlace(Side::White, 1, 0, 3, 13, WeaponType::Cannon);
            if (tick == 9) match->SubmitPlace(Side::Black, 2, 1, 9, 6, WeaponType::SCannon);
            if (tick == 30) match->SubmitPlace(Side::White, 3, 2, 6, 12, WeaponType::Infantry);
            if (tick == 40) match->SubmitPlace(Side::Black, 4, 2, 6, 7, WeaponType::Infantry);
            StepN(*match, 1);
        }
        CHECK(a.checksum() == b.checksum());
        CHECK(a.projectiles().size() == b.projectiles().size());
        CHECK(a.elixir(Side::White) == b.elixir(Side::White));
        CHECK(a.elixir(Side::Black) == b.elixir(Side::Black));
        CHECK(a.hand(Side::White) == b.hand(Side::White));
        CHECK(a.ended() == b.ended());
    }
}

TEST_CASE("infantry engages encountered enemy soldiers and releases dead targets") {
    Match match(Quick(0, 6000));
    match.SubmitPlace(Side::White, 1, 2, 6, 11, WeaponType::Infantry);
    match.SubmitPlace(Side::Black, 2, 2, 6, 8, WeaponType::Infantry);
    StepN(match, 1);
    REQUIRE(match.infantry().size() == 2 * kSquadSize);
    bool engaged = false, damaged = false, died = false;
    std::vector<InfantryState> previous;
    for (int t = 0; t < 600 && !match.ended(); t++) {
        previous = match.infantry();
        StepN(match, 1);
        died |= match.infantry().size() < 2 * kSquadSize;
        for (const auto& soldier : match.infantry()) {
            CHECK(soldier.health > 0);
            damaged |= soldier.health < kInfantryMaxHealth;
            if (soldier.phase != InfantryPhase::Attacking || soldier.target_kind != CombatTargetKind::Infantry) continue;
            engaged = true;
            const auto target = std::find_if(previous.begin(), previous.end(), [&](const InfantryState& unit) {
                return unit.id == soldier.target_id;
            });
            // A target killed this tick may be removed before the state is published.
            if (target != previous.end()) CHECK(target->side != soldier.side);
        }
    }
    CHECK(engaged);
    CHECK(damaged);
    CHECK(died);
    // Survivors must eventually resume marching, not stay locked to erased entities.
    StepN(match, 120);
    for (const auto& soldier : match.infantry()) {
        if (soldier.target_kind != CombatTargetKind::Infantry) continue;
        CHECK(std::any_of(match.infantry().begin(), match.infantry().end(), [&](const InfantryState& other) {
            return other.id == soldier.target_id && other.side != soldier.side && other.health > 0;
        }));
    }
}

TEST_CASE("advancing infantry routes around allied artillery without walking through it") {
    Match match(Quick(20, 6000));
    REQUIRE(Place(match, Side::White, 6, 11) == PlaceError::Ok);
    StepTo(match, match.start_tick());
    REQUIRE(Place(match, Side::White, 6, 14, 2) == PlaceError::Ok);
    bool passed = false;
    for (int tick = 0; tick < 360 && !match.ended(); tick++) {
        StepN(match, 1);
        for (const auto& soldier : match.infantry()) {
            const float closest_x = std::clamp(soldier.pos.x, 6.0f * kCell, 7.0f * kCell);
            const float closest_y = std::clamp(soldier.pos.y, 11.0f * kCell, 12.0f * kCell);
            const float dx = soldier.pos.x - closest_x, dy = soldier.pos.y - closest_y;
            CHECK(dx * dx + dy * dy >= kInfantryRadius * kInfantryRadius - 0.01f);
            passed |= soldier.pos.y < 11 * kCell - kInfantryRadius;
        }
    }
    CHECK(passed);
    REQUIRE(match.weapons().size() == 1);
    CHECK(match.weapons()[0].health == kWeaponMaxHealth); // allies never shoot their own blocker
}

TEST_CASE("mirrored infantry deployments advance and fight symmetrically for either side") {
    Match white(Quick(0, 6000)), black(Quick(0, 6000));
    REQUIRE(Place(white, Side::White, 3, 14, 2) == PlaceError::Ok);
    REQUIRE(Place(black, Side::Black, kCols - 1 - 3, kRows - 1 - 14, 2) == PlaceError::Ok);
    for (int tick = 0; tick < 1200; tick++) {
        StepN(white, 1);
        StepN(black, 1);
        REQUIRE(white.infantry().size() == black.infantry().size());
        for (size_t i = 0; i < white.infantry().size(); i++) {
            const auto& w = white.infantry()[i];
            const auto& b = black.infantry()[i];
            CHECK(w.pos.x + b.pos.x == doctest::Approx(kBoardW).epsilon(0.0001));
            CHECK(w.pos.y + b.pos.y == doctest::Approx(kBoardH).epsilon(0.0001));
            CHECK(w.health == b.health);
            CHECK(w.phase == b.phase);
        }
        CHECK(white.bases()[(int)Side::Black].health == black.bases()[(int)Side::White].health);
    }
}

TEST_CASE("every squad member flanks firing allies instead of queuing outside HQ range") {
    for (Side side : {Side::White, Side::Black}) {
        auto config = Quick(30, 6000);
        config.seed = 1;
        config.setup_elixir = kMaxElixirPoints;
        Match match(config);
        auto col = [&](int c) { return side == Side::White ? c : kCols - 1 - c; };
        auto row = [&](int r) { return side == Side::White ? r : kRows - 1 - r; };
        REQUIRE(Place(match, side, col(4), row(14), 0) == PlaceError::Ok);
        StepN(match, 6);
        REQUIRE(Place(match, side, col(1), row(13), 1) == PlaceError::Ok);
        StepN(match, 6);
        REQUIRE(Place(match, side, col(8), row(15), 3) == PlaceError::Ok);
        StepTo(match, match.start_tick());
        REQUIRE(Place(match, side, col(4), row(17), 2) == PlaceError::Ok);
        StepN(match, 1000);
        REQUIRE(match.infantry().size() == kSquadSize);
        for (const auto& soldier : match.infantry()) {
            CHECK(soldier.phase == InfantryPhase::Attacking);
            CHECK(soldier.target_kind == CombatTargetKind::Headquarters);
            CHECK(soldier.target_id == match.bases()[(int)Opponent(side)].id);
        }
    }
}

TEST_CASE("a soldier can retreat from a narrow corridor to flank firing allies") {
    for (Side side : {Side::White, Side::Black}) {
        auto config = Quick(0, 6000);
        config.start_elixir = kMaxElixirPoints;
        config.place_cooldown_ticks = 0;
        Match match(config);
        auto col = [&](int c) { return (uint8_t)(side == Side::White ? c : kCols - 1 - c); };
        auto row = [&](int r) { return (uint8_t)(side == Side::White ? r : kRows - 1 - r); };
        match.SubmitPlace(side, 1, 0, col(5), row(11), WeaponType::Cannon);
        match.SubmitPlace(side, 2, 1, col(7), row(11), WeaponType::SCannon);
        match.SubmitPlace(side, 3, 2, col(6), row(11), WeaponType::Infantry);
        match.SubmitPlace(Opponent(side), 4, 0, col(6), row(9), WeaponType::Cannon);
        const float rear_start = SquadPositions(side, col(6), row(11))[2].y;
        const auto results = Only<EvPlaceResult>(StepN(match, 1));
        REQUIRE(results.size() == 4);
        for (const auto& result : results) REQUIRE(result.err == PlaceError::Ok);
        REQUIRE(match.infantry().size() == kSquadSize);
        CHECK(match.infantry()[0].phase == InfantryPhase::Attacking);
        CHECK(match.infantry()[1].phase == InfantryPhase::Attacking);
        const auto& rear = match.infantry()[2];
        CHECK(rear.phase == InfantryPhase::Advancing);
        CHECK((side == Side::White ? rear.pos.y > rear_start : rear.pos.y < rear_start));
    }
}
