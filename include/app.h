#pragma once
#include <memory>
#include <string>
#include "HomeScreen.h"
#include "board_render.h"
#include "match_screen.h"
#include "sprites.h"
#include "tt/enet_link.h"
#include "tt/link.h"

struct AppOptions {
    tt::EnetLinkOptions server;              // where Find Match connects
    tt::Side practice_side = tt::Side::White;
    double sim_lag_s = 0;                    // testing: also applied to practice matches
    double sim_jitter_s = 0;
    double playback_delay_ticks = 6;         // ~100 ms behind the server
};

// The whole client: one frame loop with a home screen and a match screen.
// It owns the connection, so the network is serviced whichever screen is up.
// Construct after InitWindow and destroy before CloseWindow (it owns textures).
class App {
public:
    explicit App(const AppOptions& options);
    void Run();

private:
    void StartOnline();
    void StartPractice();
    void BackToMenu(const std::string& error = "");
    void PumpLink(double now);

    AppOptions options_;
    Atlas atlas_;
    GameSprites sprites_;
    HomeScreen home_;
    std::unique_ptr<tt::Link> link_;
    std::unique_ptr<MatchScreen> match_;
    bool find_sent_ = false;
};
