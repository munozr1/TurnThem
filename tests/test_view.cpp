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
    CHECK(result.reason == EndReason::Timer);
    CHECK(out.saw_countdown);
    CHECK(out.saw_running);
    CHECK(out.placed > 3);
    // the client's played-back board is the server's board, tile for tile
    CHECK(out.view.checksum_mismatches() == 0);
    CHECK(out.view.checksums_verified() >= 15);
    CHECK(out.view.grid().Checksum() == out.host->match().grid().Checksum());
    CHECK(result.white == out.view.grid().Count(Side::White));
    CHECK(result.black == out.view.grid().Count(Side::Black));
    CHECK(out.view.weapons().size() == out.host->match().weapons().size());
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
    CHECK(out.view.grid().at(6, 9).side == Side::White);
}
