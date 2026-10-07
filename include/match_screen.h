#pragma once
#include <string>
#include "raylib.h"
#include "board_render.h"
#include "deck.h"
#include "tt/link.h"
#include "tt/match_view.h"

// One match on screen: plays back the server's messages, lets the player drag
// cards onto the board, and draws the board, HUD, deck and overlays. The black
// player sees the board rotated 180 degrees so their side is always at the bottom.
class MatchScreen {
public:
    MatchScreen(const GameSprites& sprites, double delay_ticks);

    void OnMessage(const tt::ServerMsg& msg, double now_s);
    void Update(double now_s, tt::Link& link);
    void Draw() const;

    void SetConnectionLost(const std::string& reason);
    bool Over() const;        // result shown or connection lost
    bool WantsMenu() const { return wants_menu_; }

private:
    Rectangle MenuButton() const;
    void DrawHud() const;
    void DrawOverlay(const char* title, const std::string& detail) const;

    const GameSprites& sprites_;
    tt::MatchView view_;
    Deck deck_;
    Camera2D camera_{};
    int drag_slot_ = -1;
    bool hover_ = false;
    int hover_col_ = 0, hover_row_ = 0;
    bool hover_valid_ = false;
    std::string toast_;
    double toast_until_ = 0;
    double now_ = 0;
    int rtt_ms_ = 0;
    std::string lost_reason_;
    bool lost_ = false;
    bool wants_menu_ = false;
};
