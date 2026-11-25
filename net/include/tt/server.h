#pragma once
#include <csignal>
#include <cstdint>
#include <memory>
#include <string>
#include "tt/match.h"
#include "tt/protocol.h"

namespace tt {

struct ServerOptions {
    uint16_t port = kDefaultPort; // 0 picks a free port (tests)
    size_t max_peers = 256;       // players beyond this are told the server is full
    MatchConfig match_config;     // seed is replaced per match
    bool log = true;              // one line per match start/end (and kicks) on stdout
};

// The matchmaking + authoritative game server: pairs players first-come
// first-served, runs every match's simulation at kTickRate, and relays the
// results. Single-threaded; drive it with Run(), or Pump()/Tick() in tests.
class Server {
public:
    explicit Server(const ServerOptions& options);
    ~Server();

    bool Start(std::string* error = nullptr); // binds the UDP socket
    uint16_t port() const;                    // the bound port (after Start)

    void Pump(int timeout_ms); // handle network events, waiting up to timeout_ms for the first
    void Tick();               // pair queued players, step every match once, send, reap finished matches
    void Run(volatile std::sig_atomic_t* stop); // real-time loop at kTickRate until *stop
    void Shutdown();           // tell everyone, then close

    size_t sessions() const;
    size_t queued() const;
    size_t matches() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tt
