#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "raylib.h"
#include "app.h"

static void Usage() {
    printf("usage: TurnThem [--server host[:port]] [--practice-side white|black]\n"
           "                [--sim-lag SECONDS] [--sim-jitter SECONDS] [--delay TICKS] [--scale S]\n"
           "  --server defaults to $TURNTHEM_SERVER, then 127.0.0.1:%d\n", tt::kDefaultPort);
}

static float g_window_scale = 0; // 0 = fit the screen

static bool ParseArgs(int argc, char** argv, AppOptions& options) {
    std::string server = getenv("TURNTHEM_SERVER") ? getenv("TURNTHEM_SERVER") : "";
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--server" && has_value) server = argv[++i];
        else if (arg == "--practice-side" && has_value) {
            const std::string side = argv[++i];
            if (side != "white" && side != "black") return false;
            options.practice_side = side == "black" ? tt::Side::Black : tt::Side::White;
        } else if (arg == "--sim-lag" && has_value) options.sim_lag_s = atof(argv[++i]);
        else if (arg == "--sim-jitter" && has_value) options.sim_jitter_s = atof(argv[++i]);
        else if (arg == "--delay" && has_value) options.playback_delay_ticks = atof(argv[++i]);
        else if (arg == "--scale" && has_value) {
            g_window_scale = (float)atof(argv[++i]);
            if (g_window_scale < 0.3f || g_window_scale > 2.0f) return false;
        }
        else return false;
    }
    if (!server.empty() && !tt::ParseServerAddress(server, options.server.host, options.server.port)) {
        fprintf(stderr, "bad server address: %s\n", server.c_str());
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    AppOptions options;
    if (!ParseArgs(argc, argv, options)) {
        Usage();
        return 2;
    }
    if (!tt::NetInit()) {
        fprintf(stderr, "couldn't initialise networking\n");
        return 1;
    }

    // keep running (and talking to the server) while minimised, or the server
    // times us out and drops us from the queue or match
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(kScreenW, kScreenH, "TurnThem");
    SetTargetFPS(60);
    // 990 px is taller than many laptop screens: shrink the window to fit, keeping
    // the aspect ratio (the app scales its fixed layout to whatever size the window is)
    const int monitor = GetCurrentMonitor();
    const float fit = g_window_scale > 0 ? g_window_scale : std::min(1.0f, (GetMonitorHeight(monitor) - 120.0f) / kScreenH);
    if (fit != 1.0f) {
        const int width = (int)(kScreenW * fit), height = (int)(kScreenH * fit);
        SetWindowSize(width, height);
        SetWindowPosition((GetMonitorWidth(monitor) - width) / 2, 60);
    }
    {
        App app(options);
        app.Run();
    } // textures are released before the window closes
    CloseWindow();

    tt::NetShutdown();
    return 0;
}
