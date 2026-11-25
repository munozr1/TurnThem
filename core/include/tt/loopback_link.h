#pragma once
#include <deque>
#include <memory>
#include <random>
#include "tt/link.h"
#include "tt/match_host.h"

namespace tt {

struct LoopbackOptions {
    Side human = Side::White;
    MatchConfig config;
    bool bot_opponent = true;
    double lag_s = 0;    // added to each direction
    double jitter_s = 0; // extra random delay per message (delivery order is kept)
    uint64_t seed = 1;
};

// Runs a MatchHost in-process: practice mode, development, and tests. Every
// message still goes through Encode/Decode, and the match is stepped at
// kTickRate from the times passed to Pump(), so the client code is the same as
// online. The host is created when the client sends FIND_MATCH.
class LoopbackLink : public Link {
public:
    static constexpr int kMaxStepsPerPump = 5; // after a long stall the match runs slow rather than jumping

    explicit LoopbackLink(const LoopbackOptions& options);

    void Pump(double now_s) override;
    void Send(const ClientMsg& msg) override;
    bool Receive(ServerMsg& msg) override;
    LinkStatus status() const override { return LinkStatus::Connected; }
    int rtt_ms() const override { return (int)(2000 * options_.lag_s); }

    const MatchHost* host() const { return host_.get(); }

private:
    struct InFlight {
        double at;
        std::vector<uint8_t> bytes;
    };
    double DeliveryTime(double& last);
    void BotAct();

    LoopbackOptions options_;
    std::unique_ptr<MatchHost> host_;
    std::deque<InFlight> up_, down_;
    double last_up_ = 0, last_down_ = 0;
    std::deque<ServerMsg> inbox_;
    std::mt19937_64 rng_;
    double now_ = 0;
    double match_start_ = 0;
    uint64_t steps_ = 0;
    uint32_t bot_next_tick_ = 0;
    uint32_t bot_req_id_ = 1;
};

} // namespace tt
