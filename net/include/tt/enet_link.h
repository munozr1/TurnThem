#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include "tt/link.h"

// The client side of the network. enet.h stays out of this header on purpose:
// on Windows it pulls in windows.h, which clashes with raylib.
namespace tt {

// wrap enet_initialize / enet_deinitialize. Reference-counted, so balanced
// calls are safe; links and servers also hold a reference while they exist.
bool NetInit();
void NetShutdown();

struct EnetLinkOptions {
    std::string host = "127.0.0.1";
    uint16_t port = kDefaultPort;
    double sim_lag_s = 0;    // testing: extra delay added to each direction
    double sim_jitter_s = 0; // testing: extra random delay per message (order is kept)
    uint32_t protocol_version = kProtocolVersion; // tests override this to check rejection
};

// "host" (port = kDefaultPort) or "host:port"; false if the port isn't a number
// in 1..65535. IPv4 addresses and hostnames only. Outputs are untouched on failure.
bool ParseServerAddress(const std::string& text, std::string& host, uint16_t& port);

// Starts connecting immediately (resolving a hostname blocks briefly); status()
// is Connecting until the server accepts. Messages sent while Connecting are
// kept and sent on connect; once Closed they are dropped. Never null: if the
// socket or address fails, the link is already Closed with the reason.
std::unique_ptr<Link> MakeEnetLink(const EnetLinkOptions& options);

} // namespace tt
