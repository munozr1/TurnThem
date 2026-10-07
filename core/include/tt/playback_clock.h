#pragma once
#include <cstdint>
#include <deque>

namespace tt {

// Decides which server tick the client is showing. The server sends a TICK
// every 1/60 s; network jitter bunches them up, so the client plays back a
// little behind the newest one and smooths over the gaps.
//
// Server time is estimated from the fastest recent arrival: offset = tick -
// now*60, and the largest offset in the last few seconds is the least-delayed
// packet. Playback aims `delay` ticks behind that estimate, speeding up or
// slowing down slightly to stay there, never runs backwards, and never passes
// the newest tick received (if packets stop, it waits).
class PlaybackClock {
public:
    static constexpr double kWindowSeconds = 3.0; // how long an arrival counts toward the estimate
    static constexpr double kSnapTicks = 20.0;    // further behind than this: jump instead of catching up
    static constexpr double kMaxRateChange = 0.25;
    static constexpr double kRebaseSeconds = 0.5;  // consistently late this long: server time moved

    explicit PlaybackClock(double delay_ticks = 6.0) : delay_(delay_ticks) {}

    void Reset();
    void OnTick(uint32_t tick, double now_s);
    void Update(double now_s);

    bool started() const { return started_; }
    double playback() const { return pb_; } // fractional server tick being shown (may start negative)
    uint32_t latest() const { return latest_; }
    double delay() const { return delay_; }

private:
    struct Sample {
        double at;
        double offset;
    };

    double delay_;
    std::deque<Sample> window_; // sliding max: offsets strictly decreasing front to back
    double max_offset_ = 0;
    double low_since_ = -1; // when arrivals started being consistently late, or -1
    double last_now_ = 0;
    double pb_ = 0;
    uint32_t latest_ = 0;
    bool have_tick_ = false;
    bool started_ = false;
};

} // namespace tt
