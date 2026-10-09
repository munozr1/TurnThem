#include "app.h"
#include "tt/loopback_link.h"

App::App(const AppOptions& options)
    : options_(options),
      frame_(LoadRenderTexture(kScreenW, kScreenH)),
      atlas_("assets/sprites_texture_file.png", "assets/sprites_data_file.json"),
      sprites_(atlas_) {
    SetTextureFilter(frame_.texture, TEXTURE_FILTER_BILINEAR); // smooth when the window is scaled down
    home_.SetFooter(TextFormat("Server: %s:%d", options_.server.host.c_str(), options_.server.port));
}

App::~App() {
    UnloadRenderTexture(frame_);
}

void App::StartOnline() {
    tt::EnetLinkOptions server = options_.server;
    server.sim_lag_s = options_.sim_lag_s;
    server.sim_jitter_s = options_.sim_jitter_s;
    link_ = tt::MakeEnetLink(server);
    find_sent_ = false;
    home_.SetStatus(HomeStatus::Connecting);
}

void App::StartPractice() {
    tt::LoopbackOptions practice;
    practice.human = options_.practice_side;
    practice.lag_s = options_.sim_lag_s;
    practice.jitter_s = options_.sim_jitter_s;
    practice.seed = (uint64_t)(GetTime() * 1e6) ^ (uint64_t)GetRandomValue(0, 1 << 30);
    link_ = std::make_unique<tt::LoopbackLink>(practice);
    find_sent_ = false;
    home_.SetStatus(HomeStatus::Searching);
}

void App::BackToMenu(const std::string& error) {
    match_.reset();
    link_.reset(); // disconnects
    home_.SetStatus(HomeStatus::Menu);
    home_.SetError(error);
}

void App::PumpLink(double now) {
    if (!link_) return;
    link_->Pump(now);
    if (!find_sent_ && link_->status() == tt::LinkStatus::Connected) {
        link_->Send(tt::MsgFindMatch{});
        find_sent_ = true;
    }

    tt::ServerMsg msg;
    while (link_ && link_->Receive(msg)) {
        if (std::holds_alternative<tt::MsgQueued>(msg)) {
            home_.SetStatus(HomeStatus::Searching);
        } else if (std::holds_alternative<tt::MsgMatchStart>(msg)) {
            match_ = std::make_unique<MatchScreen>(sprites_, options_.playback_delay_ticks);
            match_->OnMessage(msg, now);
        } else if (match_) {
            match_->OnMessage(msg, now);
        }
    }

    if (link_->status() == tt::LinkStatus::Closed) {
        if (!match_) BackToMenu(link_->close_reason());
        else if (!match_->Over()) match_->SetConnectionLost(link_->close_reason());
    }
}

void App::Run() {
    SetExitKey(KEY_NULL); // Esc leaves a match instead of quitting the game
    while (!WindowShouldClose()) {
        const double now = GetTime();
        // everything below works in layout coordinates; the window may be smaller
        const float scale = (float)kScreenW / GetScreenWidth();
        SetMouseScale(scale, scale);
        PumpLink(now);

        if (IsKeyPressed(KEY_ESCAPE)) {
            if (!match_ && home_.Status() == HomeStatus::Menu) break;
            if (!match_) BackToMenu(); // cancel the search
            else if (!match_->Over()) link_->Send(tt::MsgLeaveMatch{}); // forfeit; the result overlay follows
            else BackToMenu();
        }

        if (match_) {
            match_->Update(now, *link_);
            if (match_->WantsMenu()) BackToMenu();
        } else {
            switch (home_.Update()) {
                case HomeAction::FindMatch: StartOnline(); break;
                case HomeAction::Practice: StartPractice(); break;
                case HomeAction::Cancel:
                    if (link_ && link_->status() == tt::LinkStatus::Connected) link_->Send(tt::MsgCancelFind{});
                    BackToMenu();
                    break;
                case HomeAction::None: break;
            }
        }

        BeginTextureMode(frame_);
        ClearBackground(RAYWHITE);
        if (match_) match_->Draw();
        else home_.Draw();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        // render textures are stored upside down, hence the negative height
        DrawTexturePro(frame_.texture, {0, 0, (float)kScreenW, (float)-kScreenH},
                       {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, {0, 0}, 0, WHITE);
        EndDrawing();
    }
    // closing the window mid-match drops the connection, which the server treats as leaving
}
