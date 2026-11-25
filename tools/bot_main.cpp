// TurnThemBot: headless players for testing a server. Runs N bots in one
// process, each on its own ENet connection; they queue, play with the bot
// policy and check every grid checksum. Exits non-zero on a desync, a
// protocol error or an unexpected disconnect.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include "tt/bot_client.h"
#include "tt/enet_link.h"

namespace {

void Usage() {
    std::fprintf(stderr,
                 "usage: TurnThemBot [--server host:port] [--count N] [--matches M] [--sim-lag MS] [--sim-jitter MS]\n"
                 "  --server host:port  server to play on (default $TURNTHEM_SERVER, else 127.0.0.1:%u)\n"
                 "  --count N           bots to run at once (default 2); they're paired by the server\n"
                 "  --matches M         matches each bot plays before it stops (default 1)\n"
                 "  --sim-lag MS        extra delay each way, for testing (default 0)\n"
                 "  --sim-jitter MS     extra random delay per message, order kept (default 0)\n",
                 (unsigned)tt::kDefaultPort);
}

bool ParseInt(const char* text, long min, long max, long& out) {
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < min || value > max) return false;
    out = value;
    return true;
}

const char* SideText(tt::Side side) { return side == tt::Side::White ? "white" : "black"; }

const char* ReasonText(tt::EndReason reason) {
    switch (reason) {
        case tt::EndReason::Timer: return "timer";
        case tt::EndReason::Forfeit: return "forfeit";
        case tt::EndReason::Cancelled: return "cancelled";
        default: return "?";
    }
}

const char* WinnerText(tt::Winner winner) {
    switch (winner) {
        case tt::Winner::White: return "white wins";
        case tt::Winner::Black: return "black wins";
        case tt::Winner::Draw: return "draw";
        default: return "?";
    }
}

struct Slot {
    int index = 0;
    std::unique_ptr<tt::BotClient> bot;
    int played = 0;
    bool done = false;
};

} // namespace

int main(int argc, char** argv) {
    tt::EnetLinkOptions link_options;
    const char* env = std::getenv("TURNTHEM_SERVER");
    std::string address = env && *env ? env : "127.0.0.1:" + std::to_string(tt::kDefaultPort);
    long count = 2, matches = 1, lag_ms = 0, jitter_ms = 0;
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (!std::strcmp(arg, "--help") || !std::strcmp(arg, "-h")) {
            Usage();
            return 0;
        }
        if (i + 1 >= argc) {
            std::fprintf(stderr, "%s: missing value\n", arg);
            Usage();
            return 2;
        }
        const char* value = argv[++i];
        bool ok = true;
        if (!std::strcmp(arg, "--server")) address = value;
        else if (!std::strcmp(arg, "--count")) ok = ParseInt(value, 1, 1000, count);
        else if (!std::strcmp(arg, "--matches")) ok = ParseInt(value, 1, 1000000, matches);
        else if (!std::strcmp(arg, "--sim-lag")) ok = ParseInt(value, 0, 5000, lag_ms);
        else if (!std::strcmp(arg, "--sim-jitter")) ok = ParseInt(value, 0, 5000, jitter_ms);
        else {
            std::fprintf(stderr, "unknown option %s\n", arg);
            Usage();
            return 2;
        }
        if (!ok) {
            std::fprintf(stderr, "bad value for %s: %s\n", arg, value);
            return 2;
        }
    }
    if (!tt::ParseServerAddress(address, link_options.host, link_options.port)) {
        std::fprintf(stderr, "bad server address '%s' (expected host or host:port)\n", address.c_str());
        return 2;
    }
    link_options.sim_lag_s = lag_ms / 1000.0;
    link_options.sim_jitter_s = jitter_ms / 1000.0;
    if (count % 2) std::printf("note: an odd number of bots; the last one waits for a human opponent\n");

    if (!tt::NetInit()) {
        std::fprintf(stderr, "couldn't initialize networking\n");
        return 1;
    }

    std::mt19937_64 seeds{std::random_device{}()};
    // a fresh connection per match: also exercises connect/disconnect on the server
    auto connect = [&](Slot& slot) {
        slot.bot = std::make_unique<tt::BotClient>(tt::MakeEnetLink(link_options), seeds());
    };
    std::vector<Slot> slots((size_t)count);
    for (size_t i = 0; i < slots.size(); i++) {
        slots[i].index = (int)i + 1;
        connect(slots[i]);
    }
    std::printf("%ld bots, %ld match(es) each, against %s:%u\n", count, matches, link_options.host.c_str(),
                (unsigned)link_options.port);
    std::fflush(stdout);

    int failures = 0, results = 0;
    size_t active = slots.size();
    const auto start = std::chrono::steady_clock::now();
    while (active > 0) {
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        for (Slot& slot : slots) {
            if (slot.done) continue;
            slot.bot->Pump(now);
            if (!slot.bot->finished()) continue;

            const tt::MatchView& view = slot.bot->view();
            if (auto result = slot.bot->result()) {
                results++;
                slot.played++;
                const bool desync = view.checksum_mismatches() > 0;
                std::printf("bot %d: match %u as %s: %s, %s (white %u, black %u); placed %d, rejected %d; "
                            "checksums %d ok, %d bad%s\n",
                            slot.index, view.match_id(), SideText(view.side()), ReasonText(result->reason),
                            WinnerText(result->winner), result->white, result->black, slot.bot->placed(),
                            slot.bot->rejected(), view.checksums_verified(), view.checksum_mismatches(),
                            desync ? "  <-- DESYNC" : "");
                if (desync) failures++;
                if (slot.played < matches) {
                    connect(slot);
                    continue;
                }
            } else {
                std::printf("bot %d: connection closed: %s\n", slot.index, slot.bot->link().close_reason().c_str());
                failures++;
            }
            slot.done = true;
            slot.bot.reset(); // disconnects
            active--;
        }
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(8)); // ~120 Hz
    }

    std::printf("%d results, %d failures\n", results, failures);
    tt::NetShutdown();
    return failures ? 1 : 0;
}
