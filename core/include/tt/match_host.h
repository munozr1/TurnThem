#pragma once
#include <array>
#include <vector>
#include "tt/match.h"
#include "tt/protocol.h"

namespace tt {

// One match as the server runs it: feeds client messages into the Match and
// turns its events into each player's outgoing messages. Shared by the real
// server and the in-process loopback link, so both use the real codec.
class MatchHost {
public:
    using Packet = std::vector<uint8_t>;

    MatchHost(uint32_t match_id, const MatchConfig& config); // queues MATCH_START for both sides

    void OnClientMessage(Side side, const ClientMsg& msg);
    void Forfeit(Side side);
    // one tick: TICK to both sides, then private results, then MATCH_END if it just ended
    void Step();
    std::vector<Packet> TakeOutbox(Side side);

    bool Finished() const { return match_.ended(); }
    uint32_t id() const { return id_; }
    const Match& match() const { return match_; }

private:
    void Send(Side side, const ServerMsg& msg);

    uint32_t id_;
    Match match_;
    std::vector<Event> events_;
    std::array<std::vector<Packet>, 2> outbox_;
};

} // namespace tt
