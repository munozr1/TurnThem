#pragma once
#include <string>
#include "tt/protocol.h"

namespace tt {

enum class LinkStatus { Connecting, Connected, Closed };

// The client's connection to a match server: the real one over ENet, or an
// in-process one for practice and tests. Pump() from the frame loop.
class Link {
public:
    virtual ~Link() = default;
    virtual void Pump(double now_s) = 0;
    virtual void Send(const ClientMsg& msg) = 0;
    // next message from the server, if any
    virtual bool Receive(ServerMsg& msg) = 0;
    virtual LinkStatus status() const = 0;
    virtual std::string close_reason() const { return {}; }
    virtual int rtt_ms() const { return 0; }
};

} // namespace tt
