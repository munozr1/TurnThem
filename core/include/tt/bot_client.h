#pragma once
#include <memory>
#include <optional>
#include <random>
#include "tt/link.h"
#include "tt/match_view.h"

namespace tt {

// A headless player: finds a match over any Link, places weapons with the bot
// policy, and checks every grid checksum the server sends. Used by
// TurnThemBot and the end-to-end tests.
class BotClient {
public:
    BotClient(std::unique_ptr<Link> link, uint64_t seed, double place_every_s = 0.5);

    void Pump(double now_s);
    void Leave(); // forfeit the current match

    bool finished() const;                   // match over, or the link closed
    std::optional<MsgMatchEnd> result() const { return view_.result(); }
    const MatchView& view() const { return view_; }
    Link& link() { return *link_; }
    int placed() const { return placed_; }
    int rejected() const { return rejected_; }

private:
    std::unique_ptr<Link> link_;
    MatchView view_{0.0}; // no playback delay: apply everything as it arrives
    std::mt19937_64 rng_;
    double place_every_s_;
    double next_place_ = 0;
    bool asked_ = false;
    int placed_ = 0;
    int rejected_ = 0;
};

} // namespace tt
