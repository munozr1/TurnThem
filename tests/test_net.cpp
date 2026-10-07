#include <enet/enet.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <random>
#include <thread>
#include "doctest/doctest.h"
#include "tt/bot_client.h"
#include "tt/enet_link.h"
#include "tt/server.h"

using namespace tt;

namespace {

using Clock = std::chrono::steady_clock;

ServerOptions TestServer(uint32_t countdown_ticks = 30, uint32_t duration_ticks = 5 * kTickRate) {
    ServerOptions options;
    options.port = 0;
    options.log = false;
    options.match_config.countdown_ticks = countdown_ticks;
    options.match_config.duration_ticks = duration_ticks;
    return options;
}

EnetLinkOptions LinkTo(const Server& server) {
    EnetLinkOptions options;
    options.host = "127.0.0.1";
    options.port = server.port();
    return options;
}

Winner WinnerFor(Side side) { return side == Side::White ? Winner::White : Winner::Black; }

// Runs the server faster than real time: one tick per iteration, with ~1 ms of
// wall-clock time between iterations so ENet's handshakes and acks get through.
// Clients are pumped with the server's simulated time, so their clocks agree
// with its tick rate.
struct Driver {
    Server& server;
    double sim_s = 0;

    // false if `done` isn't reached within the wall-clock limit
    template <typename Clients, typename Done>
    bool Until(Clients clients, Done done, double limit_s = 8.0) {
        const auto deadline = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(limit_s));
        while (!done()) {
            if (Clock::now() > deadline) return false;
            server.Pump(0);
            server.Tick();
            sim_s += 1.0 / kTickRate;
            clients(sim_s);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }
};

// Both bots saw the same end, and it matches each one's own copy of the board.
void CheckAgreed(const BotClient& a, const BotClient& b) {
    REQUIRE(a.result());
    REQUIRE(b.result());
    const MsgMatchEnd ra = *a.result(), rb = *b.result();
    CHECK(ra.reason == EndReason::Timer);
    CHECK(rb.reason == ra.reason);
    CHECK(rb.winner == ra.winner);
    CHECK(rb.white == ra.white);
    CHECK(rb.black == ra.black);
    CHECK(rb.end_tick == ra.end_tick);
    CHECK(ra.white + ra.black == kCols * kRows);
    CHECK(a.view().match_id() == b.view().match_id());
    CHECK(a.view().side() != b.view().side());
    for (const BotClient* bot : {&a, &b}) {
        CHECK(bot->view().grid().Count(Side::White) == ra.white);
        CHECK(bot->view().grid().Count(Side::Black) == ra.black);
        CHECK(bot->view().checksum_mismatches() == 0);
        CHECK(bot->view().checksums_verified() > 0);
        CHECK(bot->placed() > 0);
    }
    const int white = ra.white, black = ra.black;
    CHECK(ra.winner == (white > black ? Winner::White : black > white ? Winner::Black : Winner::Draw));
}

// A bare ENet client, for sending what a real client never would.
class RawClient {
public:
    explicit RawClient(uint16_t port) {
        NetInit();
        host_ = enet_host_create(nullptr, 1, 1, 0, 0);
        REQUIRE(host_);
        ENetAddress address;
        enet_address_set_host(&address, "127.0.0.1");
        address.port = port;
        peer_ = enet_host_connect(host_, &address, 1, kProtocolVersion);
        REQUIRE(peer_);
    }
    ~RawClient() {
        if (connected && !disconnected) enet_peer_disconnect_now(peer_, 0);
        enet_host_destroy(host_);
        NetShutdown();
    }

    void Pump() {
        ENetEvent event;
        while (enet_host_service(host_, &event, 0) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                connected = true;
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                ServerMsg msg;
                if (Decode(event.packet->data, event.packet->dataLength, msg)) received.push_back(msg);
                enet_packet_destroy(event.packet);
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                disconnected = true;
                code = event.data;
            }
        }
    }

    void Send(const std::vector<uint8_t>& bytes, bool flush = true) {
        ENetPacket* packet = enet_packet_create(bytes.data(), bytes.size(), ENET_PACKET_FLAG_RELIABLE);
        if (enet_peer_send(peer_, 0, packet) < 0) enet_packet_destroy(packet);
        if (flush) enet_host_flush(host_);
    }
    void Send(const ClientMsg& msg) { Send(Encode(msg)); }

    template <typename T>
    int Count() const {
        return (int)std::count_if(received.begin(), received.end(), [](const ServerMsg& m) { return std::holds_alternative<T>(m); });
    }
    // the newest TICK, or -1
    int64_t LatestTick() const {
        for (auto it = received.rbegin(); it != received.rend(); ++it)
            if (auto* tick = std::get_if<MsgTick>(&*it)) return tick->tick;
        return -1;
    }

    bool connected = false, disconnected = false;
    uint32_t code = 0;
    std::vector<ServerMsg> received;

private:
    ENetHost* host_ = nullptr;
    ENetPeer* peer_ = nullptr;
};

} // namespace

TEST_CASE("server addresses parse as host or host:port") {
    std::string host;
    uint16_t port = 0;
    CHECK(ParseServerAddress("127.0.0.1", host, port));
    CHECK(host == "127.0.0.1");
    CHECK(port == kDefaultPort);
    CHECK(ParseServerAddress("play.example.com:9000", host, port));
    CHECK(host == "play.example.com");
    CHECK(port == 9000);
    CHECK(ParseServerAddress("h:1", host, port));
    CHECK(port == 1);
    CHECK(ParseServerAddress("h:65535", host, port));
    CHECK(port == 65535);

    host = "untouched";
    port = 5;
    for (const char* bad : {"", ":7777", "h:", "h:0", "h:65536", "h:99999", "h:123456", "h:12ab", "h:-1", "h:+80",
                            "h: 80", "::1", "h:7777:1"}) {
        CAPTURE(bad);
        CHECK_FALSE(ParseServerAddress(bad, host, port));
    }
    CHECK(host == "untouched");
    CHECK(port == 5);
}

TEST_CASE("two bots play a whole match over ENet and agree with the server") {
    Server server(TestServer());
    std::string error;
    REQUIRE(server.Start(&error));
    REQUIRE(server.port() != 0);
    EnetLinkOptions link = LinkTo(server);
    SUBCASE("straight through") {}
    SUBCASE("with simulated lag and jitter") {
        link.sim_lag_s = 0.05;
        link.sim_jitter_s = 0.03;
    }

    BotClient a(MakeEnetLink(link), 1), b(MakeEnetLink(link), 2);
    Driver driver{server};
    size_t most_matches = 0, most_sessions = 0;
    REQUIRE(driver.Until(
        [&](double t) {
            a.Pump(t);
            b.Pump(t);
            most_matches = std::max(most_matches, server.matches());
            most_sessions = std::max(most_sessions, server.sessions());
        },
        [&] { return a.finished() && b.finished(); }));

    CheckAgreed(a, b);
    CHECK(most_matches == 1);
    CHECK(most_sessions == 2);
    // the match is reaped and both players are idle, still connected, free to queue again
    CHECK(server.matches() == 0);
    CHECK(server.queued() == 0);
    CHECK(server.sessions() == 2);
    CHECK(a.link().status() == LinkStatus::Connected);
    CHECK(b.link().status() == LinkStatus::Connected);
    CHECK(a.link().rtt_ms() >= 0);
}

TEST_CASE("a player who drops mid-match forfeits to the other") {
    Server server(TestServer(30, 30 * kTickRate));
    REQUIRE(server.Start());
    auto a = std::make_unique<BotClient>(MakeEnetLink(LinkTo(server)), 1);
    BotClient b(MakeEnetLink(LinkTo(server)), 2);
    Driver driver{server};
    double running_since = -1;
    REQUIRE(driver.Until(
        [&](double t) {
            a->Pump(t);
            b.Pump(t);
            if (running_since < 0 && b.view().phase() == MatchView::Phase::Running) running_since = t;
        },
        [&] { return running_since >= 0 && driver.sim_s > running_since + 1.0; }));
    REQUIRE(server.matches() == 1);
    const Side dropped = a->view().side();

    a.reset(); // the connection goes away without a LEAVE_MATCH
    REQUIRE(driver.Until([&](double t) { b.Pump(t); }, [&] { return b.finished(); }));
    REQUIRE(b.result());
    CHECK(b.result()->reason == EndReason::Forfeit);
    CHECK(b.view().side() == Opponent(dropped));
    CHECK(b.result()->winner == WinnerFor(b.view().side()));
    CHECK(b.view().checksum_mismatches() == 0);
    CHECK(b.link().status() == LinkStatus::Connected);
    CHECK(server.matches() == 0);
    CHECK(server.sessions() == 1);
}

TEST_CASE("a player who drops during the countdown cancels the match") {
    Server server(TestServer(20 * kTickRate, 30 * kTickRate));
    REQUIRE(server.Start());
    auto a = std::make_unique<BotClient>(MakeEnetLink(LinkTo(server)), 1);
    BotClient b(MakeEnetLink(LinkTo(server)), 2);
    Driver driver{server};
    REQUIRE(driver.Until(
        [&](double t) {
            a->Pump(t);
            b.Pump(t);
        },
        [&] { return b.view().phase() == MatchView::Phase::Countdown; }));
    a.reset();
    REQUIRE(driver.Until([&](double t) { b.Pump(t); }, [&] { return b.finished(); }));
    REQUIRE(b.result());
    CHECK(b.result()->reason == EndReason::Cancelled);
    CHECK(b.result()->winner == Winner::Draw);
    CHECK(server.matches() == 0);
}

TEST_CASE("a client with another protocol version is turned away") {
    Server server(TestServer());
    REQUIRE(server.Start());
    EnetLinkOptions options = LinkTo(server);
    options.protocol_version = 999;
    auto link = MakeEnetLink(options);
    Driver driver{server};
    REQUIRE(driver.Until([&](double t) { link->Pump(t); }, [&] { return link->status() == LinkStatus::Closed; }));
    CHECK(link->close_reason().find("version") != std::string::npos);
    CHECK(server.sessions() == 0);
}

TEST_CASE("a full server says so") {
    ServerOptions options = TestServer();
    options.max_peers = 1;
    Server server(options);
    REQUIRE(server.Start());
    auto first = MakeEnetLink(LinkTo(server));
    Driver driver{server};
    REQUIRE(driver.Until([&](double t) { first->Pump(t); },
                         [&] { return first->status() == LinkStatus::Connected && server.sessions() == 1; }));
    auto second = MakeEnetLink(LinkTo(server));
    REQUIRE(driver.Until(
        [&](double t) {
            first->Pump(t);
            second->Pump(t);
        },
        [&] { return second->status() == LinkStatus::Closed; }));
    CHECK(second->close_reason().find("full") != std::string::npos);
    CHECK(first->status() == LinkStatus::Connected);
    CHECK(server.sessions() == 1);
}

TEST_CASE("garbage gets a client kicked and the server carries on") {
    Server server(TestServer());
    REQUIRE(server.Start());
    Driver driver{server};

    std::mt19937_64 rng(7);
    std::vector<uint8_t> random_bytes{0x7F}; // not a message type, so it can't decode by luck
    for (int i = 0; i < 200; i++) random_bytes.push_back((uint8_t)rng());
    const std::vector<std::vector<uint8_t>> garbage = {
        {0xEE, 1, 2, 3}, // unknown message type
        {0x03, 1, 0, 0}, // truncated PLACE_REQUEST
        {0x01, 0x00},    // FIND_MATCH with trailing bytes
        {},              // empty
        random_bytes,
    };
    std::vector<std::unique_ptr<RawClient>> raws;
    for (size_t i = 0; i < garbage.size(); i++) raws.push_back(std::make_unique<RawClient>(server.port()));
    auto pump = [&](double) {
        for (auto& raw : raws) raw->Pump();
    };
    REQUIRE(driver.Until(pump, [&] { return server.sessions() == raws.size(); }));
    for (size_t i = 0; i < raws.size(); i++) raws[i]->Send(garbage[i]);
    REQUIRE(driver.Until(pump, [&] {
        return std::all_of(raws.begin(), raws.end(), [](const auto& raw) { return raw->disconnected; });
    }));
    for (size_t i = 0; i < raws.size(); i++) {
        CAPTURE(i);
        CHECK(raws[i]->code == (uint32_t)DisconnectCode::ProtocolError);
    }
    CHECK(server.sessions() == 0);

    // everyone else is still served
    BotClient a(MakeEnetLink(LinkTo(server)), 3), b(MakeEnetLink(LinkTo(server)), 4);
    REQUIRE(driver.Until(
        [&](double t) {
            a.Pump(t);
            b.Pump(t);
        },
        [&] { return a.finished() && b.finished(); }));
    CheckAgreed(a, b);
}

TEST_CASE("a player kicked mid-match forfeits it") {
    Server server(TestServer(30, 30 * kTickRate));
    REQUIRE(server.Start());
    Driver driver{server};
    RawClient raw(server.port());
    BotClient bot(MakeEnetLink(LinkTo(server)), 5);
    bool asked = false;
    uint32_t start_tick = 0;
    REQUIRE(driver.Until(
        [&](double t) {
            raw.Pump();
            bot.Pump(t);
            if (raw.connected && !asked) {
                raw.Send(MsgFindMatch{});
                asked = true;
            }
            for (const ServerMsg& msg : raw.received)
                if (auto* start = std::get_if<MsgMatchStart>(&msg)) start_tick = start->start_tick;
        },
        [&] { return start_tick > 0 && raw.LatestTick() > (int64_t)start_tick + 30; }));
    raw.Send(std::vector<uint8_t>{0xEE});
    REQUIRE(driver.Until(
        [&](double t) {
            raw.Pump();
            bot.Pump(t);
        },
        [&] { return raw.disconnected && bot.finished(); }));
    CHECK(raw.code == (uint32_t)DisconnectCode::ProtocolError);
    REQUIRE(bot.result());
    CHECK(bot.result()->reason == EndReason::Forfeit);
    CHECK(bot.result()->winner == WinnerFor(bot.view().side()));
    CHECK(server.matches() == 0);
    CHECK(server.sessions() == 1);
}

TEST_CASE("out-of-state messages are ignored, and a connection can queue again") {
    Server server(TestServer());
    REQUIRE(server.Start());
    Driver driver{server};
    RawClient raw(server.port());
    auto pump = [&](double) { raw.Pump(); };
    REQUIRE(driver.Until(pump, [&] { return raw.connected && server.sessions() == 1; }));

    // not in a match or queue: all ignored, none of them gets us kicked
    raw.Send(MsgPlaceRequest{1, 0, 6, 12, WeaponType::Cannon});
    raw.Send(MsgLeaveMatch{});
    raw.Send(MsgCancelFind{});
    raw.Send(MsgFindMatch{});
    REQUIRE(driver.Until(pump, [&] { return raw.Count<MsgQueued>() == 1 && server.queued() == 1; }));

    raw.Send(MsgFindMatch{}); // already queued
    raw.Send(MsgPlaceRequest{2, 0, 6, 12, WeaponType::Cannon});
    raw.Send(MsgCancelFind{});
    REQUIRE(driver.Until(pump, [&] { return server.queued() == 0; }));
    raw.Send(MsgFindMatch{});
    REQUIRE(driver.Until(pump, [&] { return raw.Count<MsgQueued>() == 2 && server.queued() == 1; }));
    driver.Until(pump, [] { return false; }, 0.05); // anything else in flight arrives

    CHECK(raw.Count<MsgQueued>() == 2);
    CHECK_FALSE(raw.disconnected);
    CHECK(server.sessions() == 1);
    CHECK(server.matches() == 0);
}

TEST_CASE("a client flooding the server is kicked") {
    Server server(TestServer());
    REQUIRE(server.Start());
    Driver driver{server};
    RawClient raw(server.port());
    REQUIRE(driver.Until([&](double) { raw.Pump(); }, [&] { return raw.connected && server.sessions() == 1; }));
    for (int i = 0; i < 400; i++) raw.Send(Encode(ClientMsg{MsgCancelFind{}}), false);
    raw.Send(Encode(ClientMsg{MsgCancelFind{}}));
    REQUIRE(driver.Until([&](double) { raw.Pump(); }, [&] { return raw.disconnected; }));
    CHECK(raw.code == (uint32_t)DisconnectCode::ProtocolError);
    CHECK(server.sessions() == 0);
}

TEST_CASE("the real-time loop plays a match, and shutting down tells the clients") {
    Server server(TestServer(30, 1 * kTickRate));
    REQUIRE(server.Start());
    volatile std::sig_atomic_t stop = 0;
    std::thread loop([&] { server.Run(&stop); });

    BotClient a(MakeEnetLink(LinkTo(server)), 6), b(MakeEnetLink(LinkTo(server)), 7);
    const auto start = Clock::now();
    auto seconds = [&] { return std::chrono::duration<double>(Clock::now() - start).count(); };
    while (!(a.finished() && b.finished()) && seconds() < 8) {
        a.Pump(seconds());
        b.Pump(seconds());
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    stop = 1;
    loop.join();
    CheckAgreed(a, b);
    // countdown + match is 1.5 s of ticks; at 60 Hz it can't finish much sooner
    CHECK(seconds() > 1.3);

    // the clients keep running while the server waits for their goodbyes
    std::thread clients([&] {
        while ((a.link().status() != LinkStatus::Closed || b.link().status() != LinkStatus::Closed) && seconds() < 12) {
            a.link().Pump(seconds());
            b.link().Pump(seconds());
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    server.Shutdown();
    clients.join();
    CHECK(a.link().close_reason() == "server shut down");
    CHECK(b.link().close_reason() == "server shut down");
    CHECK(server.sessions() == 0);
}
