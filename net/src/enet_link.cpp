#include "tt/enet_link.h"
#include <enet/enet.h>
#include <algorithm>
#include <cctype>
#include <deque>
#include <mutex>
#include <random>
#include <vector>

namespace tt {

namespace {

std::mutex g_net_mutex;
int g_net_refs = 0;

// ENet's own timeout only gives up on an unanswered connect after ~15 s; a
// player staring at "Connecting..." deserves an answer sooner
constexpr double kConnectTimeoutS = 5.0;

} // namespace

bool NetInit() {
    std::lock_guard<std::mutex> lock(g_net_mutex);
    if (g_net_refs == 0 && enet_initialize() != 0) return false;
    g_net_refs++;
    return true;
}

void NetShutdown() {
    std::lock_guard<std::mutex> lock(g_net_mutex);
    if (g_net_refs > 0 && --g_net_refs == 0) enet_deinitialize();
}

bool ParseServerAddress(const std::string& text, std::string& host, uint16_t& port) {
    std::string name = text;
    uint16_t number = kDefaultPort;
    const size_t colon = text.find(':');
    if (colon != std::string::npos) {
        name = text.substr(0, colon);
        const std::string digits = text.substr(colon + 1);
        // also rejects a second ':' (IPv6), which ENet 1.3 can't reach anyway
        if (digits.empty() || digits.size() > 5) return false;
        if (!std::all_of(digits.begin(), digits.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) return false;
        const unsigned long value = std::stoul(digits);
        if (value < 1 || value > 65535) return false;
        number = (uint16_t)value;
    }
    if (name.empty()) return false;
    host = name;
    port = number;
    return true;
}

namespace {

// One ENet client host with a single peer and a single reliable-ordered
// channel. Optional simulated lag/jitter delays messages in both directions
// using the times passed to Pump(), keeping their order (like LoopbackLink).
class EnetLink final : public Link {
public:
    explicit EnetLink(const EnetLinkOptions& options);
    ~EnetLink() override;

    void Pump(double now_s) override;
    void Send(const ClientMsg& msg) override;
    bool Receive(ServerMsg& msg) override;
    LinkStatus status() const override { return status_; }
    std::string close_reason() const override { return close_reason_; }
    int rtt_ms() const override;

private:
    struct Outbound {
        double at;
        std::vector<uint8_t> bytes;
    };
    struct Inbound {
        double at;
        ServerMsg msg;
    };

    double DeliveryTime(double& last);
    void SendDue();
    // the link closes once every message received before the close has been delivered
    void ScheduleClose(const std::string& reason);
    void CloseNow(const std::string& reason);
    std::string Where() const { return options_.host + ":" + std::to_string(options_.port); }

    EnetLinkOptions options_;
    bool net_ref_ = false;
    ENetHost* host_ = nullptr;
    ENetPeer* peer_ = nullptr; // null once the connection is gone
    LinkStatus status_ = LinkStatus::Connecting;
    bool ever_connected_ = false;
    std::string close_reason_;
    bool close_pending_ = false;
    double close_at_ = 0;
    std::string pending_reason_;
    std::deque<Outbound> up_; // waiting for the connection, or for simulated lag
    std::deque<Inbound> down_;
    std::deque<ServerMsg> inbox_;
    double now_ = 0;
    double connect_started_ = -1; // time of the first Pump
    double last_up_ = 0, last_down_ = 0;
    std::mt19937_64 rng_{std::random_device{}()};
};

EnetLink::EnetLink(const EnetLinkOptions& options) : options_(options) {
    net_ref_ = NetInit();
    if (!net_ref_) {
        CloseNow("couldn't initialize networking");
        return;
    }
    host_ = enet_host_create(nullptr, 1, 1, 0, 0);
    if (!host_) {
        CloseNow("couldn't open a UDP socket");
        return;
    }
    ENetAddress address;
    // resolves names with a blocking lookup; fine for "127.0.0.1" and a one-off hostname
    if (enet_address_set_host(&address, options_.host.c_str()) != 0) {
        CloseNow("couldn't find the server " + options_.host);
        return;
    }
    address.port = options_.port;
    peer_ = enet_host_connect(host_, &address, 1, options_.protocol_version);
    if (!peer_) {
        CloseNow("couldn't start connecting to " + Where());
        return;
    }
    enet_peer_timeout(peer_, 0, 5000, 15000);
    enet_host_flush(host_); // send the connect request now rather than on the first Pump
}

EnetLink::~EnetLink() {
    // tells the server straight away (it forfeits our match) instead of letting it time out
    if (peer_) enet_peer_disconnect_now(peer_, (uint32_t)DisconnectCode::None);
    if (host_) enet_host_destroy(host_);
    if (net_ref_) NetShutdown();
}

double EnetLink::DeliveryTime(double& last) {
    const double jitter = options_.sim_jitter_s * (double)(rng_() % 1000) / 1000.0;
    last = std::max(last, now_ + options_.sim_lag_s + jitter); // reliable-ordered: never overtake
    return last;
}

void EnetLink::Send(const ClientMsg& msg) {
    // after the link has closed there is nobody to send to: dropped
    if (status_ == LinkStatus::Closed || close_pending_ || !peer_) return;
    // before it connects, kept and sent on connect
    up_.push_back({DeliveryTime(last_up_), Encode(msg)});
    SendDue();
}

void EnetLink::SendDue() {
    if (status_ != LinkStatus::Connected || !peer_) return;
    bool sent = false;
    while (!up_.empty() && up_.front().at <= now_) {
        const auto& bytes = up_.front().bytes;
        ENetPacket* packet = enet_packet_create(bytes.data(), bytes.size(), ENET_PACKET_FLAG_RELIABLE);
        if (packet && enet_peer_send(peer_, 0, packet) < 0) enet_packet_destroy(packet);
        up_.pop_front();
        sent = true;
    }
    if (sent) enet_host_flush(host_); // don't wait for the next Pump: it's a game, latency matters
}

bool EnetLink::Receive(ServerMsg& msg) {
    if (inbox_.empty()) return false;
    msg = std::move(inbox_.front());
    inbox_.pop_front();
    return true;
}

int EnetLink::rtt_ms() const {
    const int simulated = (int)(2000 * options_.sim_lag_s);
    return (peer_ && status_ == LinkStatus::Connected ? (int)peer_->roundTripTime : 0) + simulated;
}

void EnetLink::ScheduleClose(const std::string& reason) {
    if (close_pending_ || status_ == LinkStatus::Closed) return;
    close_pending_ = true;
    close_at_ = DeliveryTime(last_down_);
    pending_reason_ = reason;
    up_.clear();
}

void EnetLink::CloseNow(const std::string& reason) {
    status_ = LinkStatus::Closed;
    close_reason_ = reason;
    close_pending_ = false;
    up_.clear();
    down_.clear();
    if (peer_) {
        enet_peer_disconnect_now(peer_, (uint32_t)DisconnectCode::None);
        peer_ = nullptr;
    }
    if (host_) { // nothing more will come: free the socket now
        enet_host_destroy(host_);
        host_ = nullptr;
    }
}

void EnetLink::Pump(double now_s) {
    now_ = now_s;
    if (connect_started_ < 0) connect_started_ = now_s;

    ENetEvent event;
    while (host_ && peer_ && enet_host_service(host_, &event, 0) > 0) {
        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            status_ = LinkStatus::Connected;
            ever_connected_ = true;
        } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            ServerMsg msg;
            const bool ok = Decode(event.packet->data, event.packet->dataLength, msg);
            enet_packet_destroy(event.packet);
            if (!ok) {
                // a server we can't understand: stop listening, but deliver what came before
                enet_peer_disconnect_now(peer_, (uint32_t)DisconnectCode::ProtocolError);
                peer_ = nullptr;
                ScheduleClose("protocol error");
                break;
            }
            down_.push_back({DeliveryTime(last_down_), std::move(msg)});
        } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            peer_ = nullptr; // ENet has already reset it
            std::string reason;
            if (!ever_connected_) reason = "couldn't reach the server at " + Where();
            else if (event.data == (uint32_t)DisconnectCode::None) reason = "lost connection to the server";
            else reason = DisconnectText(event.data);
            ScheduleClose(reason);
        }
    }

    if (status_ == LinkStatus::Connecting && !close_pending_ && now_ - connect_started_ >= kConnectTimeoutS) {
        CloseNow("couldn't reach the server at " + Where());
        return;
    }

    while (!down_.empty() && down_.front().at <= now_) {
        inbox_.push_back(std::move(down_.front().msg));
        down_.pop_front();
    }
    if (close_pending_ && down_.empty() && close_at_ <= now_) CloseNow(pending_reason_);

    SendDue();
}

} // namespace

std::unique_ptr<Link> MakeEnetLink(const EnetLinkOptions& options) {
    return std::make_unique<EnetLink>(options);
}

} // namespace tt
