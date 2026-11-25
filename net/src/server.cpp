#include "tt/server.h"
#include <enet/enet.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <map>
#include <random>
#include <unordered_map>
#include "tt/enet_link.h"
#include "tt/match_host.h"

namespace tt {

namespace {

using Clock = std::chrono::steady_clock;

constexpr size_t kMaxPacketSize = 4096;       // client messages are a few bytes; this bounds what a peer can make us buffer
constexpr double kMaxPacketsPerSecond = 120;  // per peer, sustained; a client sends a handful
constexpr double kPacketBurst = 120;
constexpr int kMaxCatchUpTicks = 5;           // after a stall, run late rather than in a burst
constexpr auto kStallReset = std::chrono::milliseconds(250);
constexpr int kMaxEventsPerPump = 4096;       // keep ticking even under a flood

const char* ReasonText(EndReason reason) {
    switch (reason) {
        case EndReason::Timer: return "timer";
        case EndReason::Forfeit: return "forfeit";
        case EndReason::Cancelled: return "cancelled";
        default: return "?";
    }
}

const char* WinnerText(Winner winner) {
    switch (winner) {
        case Winner::White: return "white";
        case Winner::Black: return "black";
        case Winner::Draw: return "draw";
        default: return "?";
    }
}

std::string AddressText(const ENetAddress& address) {
    char ip[64] = "?";
    enet_address_get_host_ip(&address, ip, sizeof ip);
    return std::string(ip) + ":" + std::to_string(address.port);
}

} // namespace

struct Server::Impl {
    enum class State { Idle, Queued, InMatch };

    struct Session {
        ENetPeer* peer;
        State state = State::Idle;
        uint32_t match_id = 0;
        Side side = Side::White;
        double tokens = kPacketBurst; // inbound packet budget
        Clock::time_point refilled = Clock::now();
    };

    struct MatchRecord {
        std::unique_ptr<MatchHost> host;
        std::array<Session*, 2> players{}; // by Side; null once that player has gone
    };

    explicit Impl(const ServerOptions& o) : options(o) {
        std::random_device device;
        std::seed_seq seq{device(), device(), device(), device()};
        rng.seed(seq);
    }

    void Log(const char* format, ...) {
        if (!options.log) return;
        va_list args;
        va_start(args, format);
        std::vprintf(format, args);
        va_end(args);
        std::printf("\n");
        std::fflush(stdout); // the log is usually a pipe (systemd, tests): don't sit in a buffer
    }

    Session* SessionOf(ENetPeer* peer) { return static_cast<Session*>(peer->data); }

    bool SendPacket(ENetPeer* peer, const std::vector<uint8_t>& bytes) {
        ENetPacket* packet = enet_packet_create(bytes.data(), bytes.size(), ENET_PACKET_FLAG_RELIABLE);
        if (!packet) return false;
        if (enet_peer_send(peer, 0, packet) < 0) {
            enet_packet_destroy(packet); // not queued, so still ours
            return false;
        }
        return true;
    }

    // take the session out of the queue or its match (the match is forfeited)
    void Detach(Session& session) {
        if (session.state == State::Queued) {
            queue.erase(std::remove(queue.begin(), queue.end(), &session), queue.end());
        } else if (session.state == State::InMatch) {
            auto it = matches.find(session.match_id);
            if (it != matches.end()) {
                it->second.players[(int)session.side] = nullptr;
                it->second.host->Forfeit(session.side);
            }
        }
        session.state = State::Idle;
        session.match_id = 0;
    }

    void Drop(ENetPeer* peer) {
        Session* session = SessionOf(peer);
        if (!session) return;
        Detach(*session);
        peer->data = nullptr; // ENet never clears it, and the peer slot gets reused
        sessions.erase(peer);
    }

    void Kick(ENetPeer* peer, DisconnectCode code, const char* why) {
        Log("kicked %s: %s", AddressText(peer->address).c_str(), why);
        Drop(peer);
        enet_peer_disconnect(peer, (uint32_t)code); // its DISCONNECT event will find no session
    }

    bool TakeToken(Session& session) {
        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - session.refilled).count();
        session.refilled = now;
        session.tokens = std::min(kPacketBurst, session.tokens + elapsed * kMaxPacketsPerSecond);
        if (session.tokens < 1) return false;
        session.tokens -= 1;
        return true;
    }

    void OnConnect(ENetPeer* peer, uint32_t version) {
        peer->data = nullptr;
        if (version != kProtocolVersion) {
            Log("refused %s: protocol version %u, expected %u", AddressText(peer->address).c_str(), version, kProtocolVersion);
            enet_peer_disconnect(peer, (uint32_t)DisconnectCode::VersionMismatch);
            return;
        }
        // ENet has one more slot than max_peers so that a full server can say so instead of going quiet
        if (sessions.size() >= options.max_peers) {
            Log("refused %s: server full (%zu players)", AddressText(peer->address).c_str(), sessions.size());
            enet_peer_disconnect(peer, (uint32_t)DisconnectCode::ServerFull);
            return;
        }
        enet_peer_timeout(peer, 0, 3000, 10000);
        auto session = std::make_unique<Session>();
        session->peer = peer;
        peer->data = session.get();
        sessions[peer] = std::move(session);
    }

    void OnReceive(ENetPeer* peer, ENetPacket* packet) {
        ClientMsg msg;
        const bool ok = Decode(packet->data, packet->dataLength, msg);
        enet_packet_destroy(packet);
        Session* session = SessionOf(peer);
        if (!session) return; // refused or kicked, and still talking
        if (!ok) return Kick(peer, DisconnectCode::ProtocolError, "malformed message");
        if (!TakeToken(*session)) return Kick(peer, DisconnectCode::ProtocolError, "too many packets");
        Handle(*session, msg);
    }

    // Well-formed messages that don't fit the session's state are ignored, not
    // punished: a PLACE_REQUEST that crosses MATCH_END on the wire is normal.
    void Handle(Session& session, const ClientMsg& msg) {
        if (std::holds_alternative<MsgFindMatch>(msg)) {
            if (session.state != State::Idle) return;
            session.state = State::Queued;
            queue.push_back(&session);
            SendPacket(session.peer, Encode(ServerMsg{MsgQueued{}}));
        } else if (std::holds_alternative<MsgCancelFind>(msg)) {
            if (session.state == State::Queued) Detach(session);
        } else if (session.state == State::InMatch) { // PLACE_REQUEST, LEAVE_MATCH
            auto it = matches.find(session.match_id);
            if (it != matches.end()) it->second.host->OnClientMessage(session.side, msg);
        }
    }

    void StartMatches() {
        while (queue.size() >= 2) {
            std::array<Session*, 2> players{queue[0], queue[1]};
            queue.erase(queue.begin(), queue.begin() + 2);
            if (rng() & 1) std::swap(players[0], players[1]); // sides are random, not by arrival

            const uint32_t id = next_match_id++;
            MatchConfig config = options.match_config;
            config.seed = rng();
            MatchRecord record;
            record.host = std::make_unique<MatchHost>(id, config);
            record.players = players;
            for (Side side : {Side::White, Side::Black}) {
                Session* player = players[(int)side];
                player->state = State::InMatch;
                player->match_id = id;
                player->side = side;
            }
            matches.emplace(id, std::move(record));
            Log("match %u started: seed %llu, %zu running", id, (unsigned long long)config.seed, matches.size());
        }
    }

    void LogEnd(uint32_t id, const std::vector<MatchHost::Packet>& last_packets) {
        // MATCH_END is the last thing a finished match sends to each side
        ServerMsg msg;
        if (last_packets.empty() || !Decode(last_packets.back().data(), last_packets.back().size(), msg) ||
            !std::holds_alternative<MsgMatchEnd>(msg)) {
            Log("match %u ended", id);
            return;
        }
        const MsgMatchEnd& end = std::get<MsgMatchEnd>(msg);
        Log("match %u ended: %s, winner %s, white %u black %u, tick %u", id, ReasonText(end.reason),
            WinnerText(end.winner), end.white, end.black, end.end_tick);
    }

    void StepMatches() {
        for (auto it = matches.begin(); it != matches.end();) {
            MatchRecord& record = it->second;
            record.host->Step();
            std::vector<MatchHost::Packet> white_packets;
            for (Side side : {Side::White, Side::Black}) {
                auto packets = record.host->TakeOutbox(side);
                for (const auto& packet : packets) {
                    Session* player = record.players[(int)side];
                    if (!player) break; // gone (possibly kicked just now)
                    if (!SendPacket(player->peer, packet)) {
                        // it would miss part of the match and desync; better that it knows
                        Kick(player->peer, DisconnectCode::ProtocolError, "couldn't send to it");
                    }
                }
                if (side == Side::White) white_packets = std::move(packets);
            }
            if (!record.host->Finished()) {
                ++it;
                continue;
            }
            LogEnd(it->first, white_packets);
            for (Session* player : record.players) {
                if (!player) continue;
                player->state = State::Idle; // free to FIND_MATCH again on the same connection
                player->match_id = 0;
            }
            it = matches.erase(it);
        }
    }

    ServerOptions options;
    ENetHost* host = nullptr;
    uint16_t bound_port = 0;
    bool net_ref = false;
    std::unordered_map<ENetPeer*, std::unique_ptr<Session>> sessions;
    std::deque<Session*> queue;
    std::map<uint32_t, MatchRecord> matches; // stepped in id order
    uint32_t next_match_id = 1;
    std::mt19937_64 rng;
};

Server::Server(const ServerOptions& options) : impl_(std::make_unique<Impl>(options)) {}

Server::~Server() {
    if (impl_->host) {
        // no time to wait for acks here: tell everyone once and go
        for (size_t i = 0; i < impl_->host->peerCount; i++) {
            ENetPeer* peer = &impl_->host->peers[i];
            peer->data = nullptr;
            if (peer->state != ENET_PEER_STATE_DISCONNECTED)
                enet_peer_disconnect_now(peer, (uint32_t)DisconnectCode::Shutdown);
        }
        enet_host_destroy(impl_->host);
    }
    if (impl_->net_ref) NetShutdown();
}

bool Server::Start(std::string* error) {
    auto fail = [&](const std::string& text) {
        if (error) *error = text;
        return false;
    };
    if (impl_->host) return fail("already started");
    if (!impl_->net_ref) {
        if (!NetInit()) return fail("couldn't initialize networking");
        impl_->net_ref = true;
    }
    impl_->options.max_peers = std::max<size_t>(1, impl_->options.max_peers);
    const size_t slots = std::min<size_t>(impl_->options.max_peers + 1, ENET_PROTOCOL_MAXIMUM_PEER_ID);

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = impl_->options.port;
    impl_->host = enet_host_create(&address, slots, 1, 0, 0);
    if (!impl_->host) return fail("couldn't open UDP port " + std::to_string(impl_->options.port) + " (already in use?)");
    impl_->host->maximumPacketSize = kMaxPacketSize;

    ENetAddress bound;
    impl_->bound_port = enet_socket_get_address(impl_->host->socket, &bound) == 0 ? bound.port : impl_->options.port;
    return true;
}

uint16_t Server::port() const { return impl_->bound_port; }
size_t Server::sessions() const { return impl_->sessions.size(); }
size_t Server::queued() const { return impl_->queue.size(); }
size_t Server::matches() const { return impl_->matches.size(); }

void Server::Pump(int timeout_ms) {
    ENetHost* host = impl_->host;
    if (!host) return;
    ENetEvent event;
    int result = enet_host_service(host, &event, (enet_uint32)std::max(0, timeout_ms));
    for (int n = 0; result > 0 && n < kMaxEventsPerPump; n++) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: impl_->OnConnect(event.peer, event.data); break;
            case ENET_EVENT_TYPE_RECEIVE: impl_->OnReceive(event.peer, event.packet); break;
            case ENET_EVENT_TYPE_DISCONNECT: impl_->Drop(event.peer); break;
            case ENET_EVENT_TYPE_NONE: break;
        }
        result = enet_host_service(host, &event, 0);
    }
    // anything queued while handling events (QUEUED replies, disconnects) goes out now
    enet_host_flush(host);
}

void Server::Tick() {
    if (!impl_->host) return;
    impl_->StartMatches();
    impl_->StepMatches();
    enet_host_flush(impl_->host);
}

void Server::Run(volatile std::sig_atomic_t* stop) {
    const auto step = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / kTickRate));
    auto next = Clock::now();
    while (!*stop && impl_->host) {
        const auto wait = std::chrono::ceil<std::chrono::milliseconds>(next - Clock::now()).count();
        Pump((int)std::clamp<long long>(wait, 0, 1000));
        auto now = Clock::now();
        for (int n = 0; now >= next && n < kMaxCatchUpTicks; n++) {
            Tick();
            next += step;
            now = Clock::now();
        }
        if (now - next > kStallReset) next = now; // a long stall (debugger, suspend): don't try to make it up
    }
}

void Server::Shutdown() {
    ENetHost* host = impl_->host;
    if (!host) return;
    if (!impl_->matches.empty()) impl_->Log("shutting down: %zu matches abandoned", impl_->matches.size());

    for (size_t i = 0; i < host->peerCount; i++) {
        ENetPeer* peer = &host->peers[i];
        peer->data = nullptr;
        if (peer->state != ENET_PEER_STATE_DISCONNECTED) enet_peer_disconnect(peer, (uint32_t)DisconnectCode::Shutdown);
    }
    impl_->queue.clear();
    impl_->matches.clear();
    impl_->sessions.clear();

    // give the goodbyes a moment to be acknowledged, so clients hear "shut down" rather than time out
    const auto deadline = Clock::now() + std::chrono::seconds(1);
    auto busy = [host] {
        for (size_t i = 0; i < host->peerCount; i++)
            if (host->peers[i].state != ENET_PEER_STATE_DISCONNECTED) return true;
        return false;
    };
    while (busy() && Clock::now() < deadline) {
        ENetEvent event;
        const int result = enet_host_service(host, &event, 50);
        if (result < 0) break;
        if (result > 0 && event.type == ENET_EVENT_TYPE_RECEIVE) enet_packet_destroy(event.packet);
        if (result > 0 && event.type == ENET_EVENT_TYPE_CONNECT) enet_peer_disconnect(event.peer, (uint32_t)DisconnectCode::Shutdown);
    }
    enet_host_destroy(host);
    impl_->host = nullptr;
    if (impl_->net_ref) {
        NetShutdown();
        impl_->net_ref = false;
    }
}

} // namespace tt
