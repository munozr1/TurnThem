#include "tt/playback_clock.h"
#include <algorithm>
#include "tt/types.h"

namespace tt {

void PlaybackClock::Reset() {
    window_.clear();
    max_offset_ = 0;
    last_now_ = 0;
    pb_ = 0;
    latest_ = 0;
    have_tick_ = false;
    started_ = false;
}

void PlaybackClock::OnTick(uint32_t tick, double now_s) {
    latest_ = have_tick_ ? std::max(latest_, tick) : tick;
    have_tick_ = true;
    const double offset = tick - now_s * kTickRate;
    while (!window_.empty() && window_.back().offset <= offset) window_.pop_back();
    window_.push_back({now_s, offset});
    max_offset_ = window_.front().offset;
}

void PlaybackClock::Update(double now_s) {
    if (!have_tick_) return;
    // forget old arrivals, but keep the newest so a long gap still has an estimate
    while (window_.size() > 1 && window_.front().at < now_s - kWindowSeconds) window_.pop_front();
    max_offset_ = window_.front().offset;

    const double target = now_s * kTickRate + max_offset_ - delay_;
    if (!started_) {
        pb_ = std::min(target, (double)latest_);
        last_now_ = now_s;
        started_ = true;
        return;
    }

    const double dt = std::max(0.0, now_s - last_now_);
    last_now_ = now_s;
    const double err = target - pb_;
    double next;
    if (err > kSnapTicks) {
        next = target;
    } else {
        const double rate = 1.0 + std::clamp(0.02 * err, -kMaxRateChange, kMaxRateChange);
        next = pb_ + dt * kTickRate * rate;
    }
    pb_ = std::max(pb_, std::min(next, (double)latest_));
}

} // namespace tt
