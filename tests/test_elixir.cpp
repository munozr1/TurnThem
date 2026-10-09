#include <vector>
#include "doctest/doctest.h"
#include "tt/match.h"

using namespace tt;

namespace {

MatchConfig Config(uint32_t countdown = 10, uint32_t duration = 6000) {
    MatchConfig config;
    config.seed = 3;
    config.countdown_ticks = countdown;
    config.duration_ticks = duration;
    return config;
}

PlaceError Place(Match& match, Side side, int col, int row) {
    match.SubmitPlace(side, 1, 0, (uint8_t)col, (uint8_t)row, match.hand(side)[0]);
    std::vector<Event> events;
    match.Step(events);
    for (const Event& e : events)
        if (auto* r = std::get_if<EvPlaceResult>(&e)) return r->err;
    FAIL("no placement result");
    return PlaceError::Count;
}

void StepTo(Match& match, uint32_t tick) {
    std::vector<Event> events;
    while (match.tick() < tick) match.Step(events);
}

} // namespace

TEST_CASE("elixir regenerates only during play, twice as fast in the last minute") {
    const uint32_t start = 180, end = 180 + 180 * kTickRate;
    CHECK(RegenOnTick(0, start, end) == 0);           // countdown
    CHECK(RegenOnTick(start - 1, start, end) == 0);
    CHECK(RegenOnTick(start, start, end) == 1);
    CHECK(RegenOnTick(end - kDoubleElixirTicks - 1, start, end) == 1);
    CHECK(RegenOnTick(end - kDoubleElixirTicks, start, end) == 2);
    CHECK(RegenOnTick(end - 1, start, end) == 2);
    CHECK(RegenOnTick(end, start, end) == 0);         // over
    CHECK(Regenerate(kMaxElixirPoints - 1, end - 1, start, end) == kMaxElixirPoints); // capped
    CHECK(ElixirToPoints(1) == 168); // 2.8 s per elixir at 60 ticks/s
}

TEST_CASE("a match starts at 5 elixir, holds it through the countdown, then gains 1 per 2.8 s up to 10") {
    Match match(Config());
    CHECK(match.elixir(Side::White) == ElixirToPoints(5));
    StepTo(match, match.start_tick());
    CHECK(match.elixir(Side::White) == ElixirToPoints(5));
    StepTo(match, match.start_tick() + kElixirTicks);
    CHECK(match.elixir(Side::White) == ElixirToPoints(6));
    CHECK(match.elixir(Side::Black) == ElixirToPoints(6));
    StepTo(match, match.start_tick() + 20 * kElixirTicks);
    CHECK(match.elixir(Side::White) == kMaxElixirPoints);
}

TEST_CASE("placing a card costs 3 elixir and is refused when you can't afford it") {
    Match match(Config());
    StepTo(match, match.start_tick());
    REQUIRE(Place(match, Side::White, 2, 15) == PlaceError::Ok);
    // 5 elixir, +1 point on the placement tick (regen comes first), -3 elixir
    const ElixirPoints after = ElixirToPoints(5) + 1 - CardCostPoints(WeaponType::Cannon);
    CHECK(match.elixir(Side::White) == after);

    StepTo(match, match.tick() + 10); // past the rate limit
    const Hand before = match.hand(Side::White);
    CHECK(Place(match, Side::White, 8, 15) == PlaceError::NotEnoughElixir);
    CHECK(match.hand(Side::White) == before); // nothing spent, no card drawn
    CHECK(match.grid().at(8, 15).occupied == false);

    // exactly when the elixir is there, it goes through
    const ElixirPoints missing = CardCostPoints(WeaponType::Cannon) - match.elixir(Side::White);
    StepTo(match, match.tick() + missing - 1); // the Place() step itself adds the last point
    CHECK(Place(match, Side::White, 8, 15) == PlaceError::Ok);
    CHECK(match.elixir(Side::White) == 0);
}

TEST_CASE("board rules are reported before elixir") {
    MatchConfig poor = Config();
    poor.start_elixir = 0;
    Match broke(poor);
    StepTo(broke, broke.start_tick());
    CHECK(Place(broke, Side::White, 6, 5) == PlaceError::NotOwnTile);
    CHECK(Place(broke, Side::White, 6, 15) == PlaceError::NotEnoughElixir);
}

TEST_CASE("the last minute's double elixir is visible in a short match") {
    // a 60 s match is all "last minute"
    Match match(Config(10, 60 * kTickRate));
    StepTo(match, match.start_tick() + kElixirTicks);
    CHECK(match.elixir(Side::White) == ElixirToPoints(7));
}

TEST_CASE("elixir caps how fast a player can fill the board") {
    // a player who places whenever they can afford to, for a whole 3-minute match
    Match match(Config(10, 180 * kTickRate));
    int placed = 0;
    std::vector<Event> events;
    while (!match.ended()) {
        if (match.tick() >= match.start_tick() && match.elixir(Side::White) >= CardCostPoints(WeaponType::Cannon)) {
            // first legal white cell, scanning from the front line
            for (int row = 0; row < kRows; row++) {
                bool done = false;
                for (int col = 0; col < kCols && !done; col++) {
                    if (ValidatePlacement(match.grid(), Side::White, col, row) == PlaceError::Ok) {
                        match.SubmitPlace(Side::White, 1, 0, (uint8_t)col, (uint8_t)row, match.hand(Side::White)[0]);
                        done = true;
                    }
                }
                if (done) break;
            }
        }
        const size_t before = events.size();
        match.Step(events);
        for (size_t i = before; i < events.size(); i++)
            if (auto* r = std::get_if<EvPlaceResult>(&events[i])) placed += r->err == PlaceError::Ok;
    }
    // 5 to start, ~43 over two normal minutes, ~43 in the double minute: ~90 elixir, ~30 cards
    CHECK(placed >= 25);
    CHECK(placed <= 31);
}
