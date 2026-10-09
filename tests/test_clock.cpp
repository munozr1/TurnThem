#include <math.h>
#include <random>
#include <vector>
#include "doctest/doctest.h"
#include "tt/playback_clock.h"
#include "tt/types.h"

using namespace tt;

namespace {

struct Arrival {
    double at;
    uint32_t tick;
};

// feeds arrivals in order and samples the clock at `fps`; checks the invariants on every frame
struct Run {
    std::vector<double> lag;    // latest - playback, per frame
    std::vector<double> behind; // true server tick (now*60) - playback, per frame
    std::vector<double> times;

    Run(PlaybackClock& clock, const std::vector<Arrival>& arrivals, double until, double fps = 144) {
        size_t next = 0;
        double last_pb = -1e9;
        for (double now = 0; now < until; now += 1.0 / fps) {
            while (next < arrivals.size() && arrivals[next].at <= now) clock.OnTick(arrivals[next++].tick, now);
            clock.Update(now);
            if (!clock.started()) continue;
            REQUIRE(clock.playback() >= last_pb);               // never backwards
            REQUIRE(clock.playback() <= clock.latest() + 1e-9); // never past what we have
            last_pb = clock.playback();
            lag.push_back(clock.latest() - clock.playback());
            behind.push_back(now * kTickRate - clock.playback());
            times.push_back(now);
        }
    }

    static double MeanAfter(const std::vector<double>& values, const std::vector<double>& times, double t) {
        double sum = 0;
        int n = 0;
        for (size_t i = 0; i < values.size(); i++) {
            if (times[i] >= t) {
                sum += values[i];
                n++;
            }
        }
        return sum / n;
    }
    double MeanLagAfter(double t) const { return MeanAfter(lag, times, t); }
    double MeanBehindAfter(double t) const { return MeanAfter(behind, times, t); }
};

std::vector<Arrival> Steady(double latency, double jitter, uint32_t ticks, uint64_t seed = 1) {
    std::mt19937_64 rng(seed);
    std::vector<Arrival> out;
    double last = 0;
    for (uint32_t t = 0; t < ticks; t++) {
        const double at = std::max(last, (double)t / kTickRate + latency + jitter * (double)(rng() % 1000) / 1000.0);
        out.push_back({at, t});
        last = at; // reliable-ordered delivery
    }
    return out;
}

} // namespace

TEST_CASE("steady arrivals settle at the configured delay") {
    PlaybackClock clock(6);
    Run run(clock, Steady(0.05, 0, 600), 10);
    // one-way latency (3 ticks) plus the delay, give or take one frame of arrival detection
    CHECK(run.MeanBehindAfter(2) == doctest::Approx(9).epsilon(0.06));
    // and it is smooth: after settling, the gap to server time barely moves
    double lo = 1e9, hi = -1e9;
    for (size_t i = 0; i < run.behind.size(); i++) {
        if (run.times[i] < 2 || run.times[i] > 9.5) continue;
        lo = std::min(lo, run.behind[i]);
        hi = std::max(hi, run.behind[i]);
    }
    CHECK(hi - lo < 0.6);
}

TEST_CASE("jitter is absorbed: playback stays smooth and near the delay") {
    PlaybackClock clock(6);
    Run run(clock, Steady(0.08, 0.04, 1200, 7), 20);
    const double mean = run.MeanLagAfter(3);
    CHECK(mean > 3);
    CHECK(mean < 10);
}

TEST_CASE("after a stall playback waits, then recovers quickly") {
    auto arrivals = Steady(0.05, 0, 900);
    for (auto& a : arrivals) // ticks 300..330 held up and delivered in a burst 0.5 s late
        if (a.tick >= 300 && a.tick <= 330) a.at = 5.6;
    PlaybackClock clock(6);
    Run run(clock, arrivals, 15);
    double worst_after_recovery = 0;
    for (size_t i = 0; i < run.lag.size(); i++)
        if (run.times[i] > 5.6 + 2.0 && run.times[i] < 14) worst_after_recovery = std::max(worst_after_recovery, fabs(run.lag[i] - 6));
    CHECK(worst_after_recovery < 1.5);
}

TEST_CASE("playback starts behind the first tick") {
    PlaybackClock clock(6);
    clock.OnTick(0, 1.0);
    clock.Update(1.0);
    CHECK(clock.started());
    CHECK(clock.playback() == doctest::Approx(-6));
}

TEST_CASE("when server time shifts for good, playback rebuilds its buffer in about a second") {
    // the server stalls 0.4 s at t=5 and carries on without catching up, so
    // every later tick arrives 0.4 s later than the old ones predicted
    auto arrivals = Steady(0.05, 0, 900);
    for (auto& a : arrivals)
        if (a.tick >= 300) a.at += 0.4;
    PlaybackClock clock(6);
    Run run(clock, arrivals, 14);
    double sum = 0;
    int n = 0;
    for (size_t i = 0; i < run.lag.size(); i++) {
        if (run.times[i] > 7.4 && run.times[i] < 8.4) { // 2-3 s after the shift
            sum += run.lag[i];
            n++;
        }
    }
    // the buffer is back near the 6-tick delay; without the rebase it stays near 0
    // (playback pinned to the newest tick) until the stale sample expires at ~8.4 s
    CHECK(sum / n > 4.0); // ~4.6 with the rebase, ~0.6 without
}
