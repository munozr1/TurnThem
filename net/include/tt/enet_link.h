#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include "tt/link.h"

// The client side of the network. enet.h stays out of this header on purpose:
// on Windows it pulls in windows.h, which clashes with raylib.
namespace tt {

// wrap enet_initialize / enet_deinitialize; call once per process
bool NetInit();
void NetShutdown();

struct EnetLinkOptions {
    std::string host = "127.0.0.1";
    uint16_t port = kDefaultPort;
    double sim_lag_s = 0;    // testing: extra delay added to each direction
    double sim_jitter_s = 0; // testing: extra random delay per message (order is kept)
    uint32_t protocol_version = kProtocolVersion; // tests override this to check rejection
};

// "host", "host:port"; false if the port isn't a number in 1..65535
bool ParseServerAddress(const std::string& text, std::string& host, uint16_t& port);

// Starts connecting immediately; status() is Connecting until the server accepts.
std::unique_ptr<Link> MakeEnetLink(const EnetLinkOptions& options);

} // namespace tt
