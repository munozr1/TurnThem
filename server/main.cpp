// TurnThemServer: the matchmaking + authoritative game server. Headless (no
// raylib), single-threaded; one log line per match start and end.
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "tt/enet_link.h"
#include "tt/server.h"

namespace {

volatile std::sig_atomic_t g_stop = 0;

void OnSignal(int) { g_stop = 1; }

void Usage() {
    std::fprintf(stderr,
                 "usage: TurnThemServer [--port N] [--match-seconds S] [--countdown-seconds S] [--max-players N]\n"
                 "  --port N               UDP port to listen on (default %u; 0 picks a free one)\n"
                 "  --match-seconds S      length of a match (default 180)\n"
                 "  --countdown-seconds S  countdown before play starts (default 3)\n"
                 "  --max-players N        connected players allowed at once (default 256)\n",
                 (unsigned)tt::kDefaultPort);
}

bool ParseNumber(const char* text, double min, double max, double& out) {
    char* end = nullptr;
    const double value = std::strtod(text, &end);
    if (end == text || *end != '\0' || !std::isfinite(value) || value < min || value > max) return false;
    out = value;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    tt::ServerOptions options;
    double match_s = options.match_config.duration_ticks / (double)tt::kTickRate;
    double countdown_s = options.match_config.countdown_ticks / (double)tt::kTickRate;
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
        double number = 0;
        bool ok = false;
        if (!std::strcmp(arg, "--port")) {
            ok = ParseNumber(value, 0, 65535, number) && number == std::floor(number);
            options.port = (uint16_t)number;
        } else if (!std::strcmp(arg, "--match-seconds")) {
            ok = ParseNumber(value, 1, 24 * 3600, match_s);
        } else if (!std::strcmp(arg, "--countdown-seconds")) {
            ok = ParseNumber(value, 0, 600, countdown_s);
        } else if (!std::strcmp(arg, "--max-players")) {
            ok = ParseNumber(value, 2, 4000, number) && number == std::floor(number);
            options.max_peers = (size_t)number;
        } else {
            std::fprintf(stderr, "unknown option %s\n", arg);
            Usage();
            return 2;
        }
        if (!ok) {
            std::fprintf(stderr, "bad value for %s: %s\n", arg, value);
            return 2;
        }
    }
    options.match_config.duration_ticks = (uint32_t)std::lround(match_s * tt::kTickRate);
    options.match_config.countdown_ticks = (uint32_t)std::lround(countdown_s * tt::kTickRate);

    if (!tt::NetInit()) {
        std::fprintf(stderr, "couldn't initialize networking\n");
        return 1;
    }
    int status = 0;
    {
        tt::Server server(options);
        std::string error;
        if (!server.Start(&error)) {
            std::fprintf(stderr, "couldn't start the server: %s\n", error.c_str());
            status = 1;
        } else {
            std::signal(SIGINT, OnSignal);
            std::signal(SIGTERM, OnSignal);
            std::printf("listening on UDP port %u (%g s matches, %g s countdown, up to %zu players)\n",
                        (unsigned)server.port(), match_s, countdown_s, options.max_peers);
            std::fflush(stdout);
            server.Run(&g_stop);
            std::printf("shutting down\n");
            std::fflush(stdout);
            server.Shutdown();
            std::printf("stopped\n");
        }
    }
    tt::NetShutdown();
    return status;
}
