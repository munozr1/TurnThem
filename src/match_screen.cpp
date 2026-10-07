#include "match_screen.h"
#include <math.h>

namespace {

const Color kWhiteTile = {240, 240, 240, 255};
const Color kBlackTile = {60, 60, 60, 255};

Color SideColor(tt::Side side) { return side == tt::Side::White ? kWhiteTile : kBlackTile; }

void DrawCentered(const char* text, int y, int size, Color color) {
    DrawText(text, (kScreenW - MeasureText(text, size)) / 2, y, size, color);
}

void DrawSwatch(int x, int y, tt::Side side) {
    DrawRectangle(x, y, 14, 14, SideColor(side));
    DrawRectangleLines(x, y, 14, 14, LIGHTGRAY);
}

} // namespace

MatchScreen::MatchScreen(const GameSprites& sprites, double delay_ticks)
    : sprites_(sprites), view_(delay_ticks), deck_(sprites, (float)kDeckY) {
    camera_.zoom = 1.0f;
}

void MatchScreen::OnMessage(const tt::ServerMsg& msg, double now_s) {
    view_.OnMessage(msg, now_s);
}

bool MatchScreen::Over() const {
    return lost_ || view_.phase() == tt::MatchView::Phase::Ended;
}

void MatchScreen::SetConnectionLost(const std::string& reason) {
    lost_ = true;
    lost_reason_ = reason;
}

Rectangle MatchScreen::MenuButton() const {
    return {(kScreenW - 200) / 2.0f, 470, 200, 56};
}

void MatchScreen::Update(double now_s, tt::Link& link) {
    now_ = now_s;
    rtt_ms_ = link.rtt_ms();
    view_.Update(now_s);
    if (auto rejection = view_.TakeRejection()) {
        toast_ = tt::PlaceErrorText(*rejection);
        toast_until_ = now_s + 1.5;
    }

    // black sees the board turned around so their side is at the bottom
    if (view_.side() == tt::Side::Black) {
        camera_.offset = {(float)tt::kBoardW, (float)tt::kBoardH};
        camera_.rotation = 180.0f;
    } else {
        camera_.offset = {0, 0};
        camera_.rotation = 0.0f;
    }

    const Vector2 mouse = GetMousePosition();
    if (Over()) {
        drag_slot_ = -1;
        hover_ = false;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, MenuButton())) wants_menu_ = true;
        return;
    }

    const auto& hand = view_.hand();
    if (drag_slot_ < 0) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) drag_slot_ = deck_.SlotAt(mouse, hand);
        if (drag_slot_ < 0) return;
    }
    if (!hand[drag_slot_].type || hand[drag_slot_].pending) { // the slot changed under us
        drag_slot_ = -1;
        hover_ = false;
        return;
    }

    hover_ = false;
    if (mouse.x >= 0 && mouse.x < kScreenW && mouse.y >= 0 && mouse.y < kHudY) {
        const Vector2 world = GetScreenToWorld2D(mouse, camera_);
        if (tt::CellAt({world.x, world.y}, hover_col_, hover_row_)) {
            hover_ = true;
            hover_valid_ = view_.CheckLocal(hover_col_, hover_row_) == tt::PlaceError::Ok;
        }
    }

    // released (checking "not down" also catches a release that happened between frames)
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (hover_ && hover_valid_) {
            if (auto request = view_.RequestPlace((uint8_t)drag_slot_, hover_col_, hover_row_)) link.Send(*request);
        } else if (hover_) {
            toast_ = tt::PlaceErrorText(view_.CheckLocal(hover_col_, hover_row_));
            toast_until_ = now_s + 1.5;
        }
        drag_slot_ = -1;
        hover_ = false;
    }
}

void MatchScreen::Draw() const {
    BeginMode2D(camera_);
    DrawBoard(view_.grid());
    if (drag_slot_ >= 0 && hover_) DrawHover(hover_col_, hover_row_, hover_valid_);
    for (const tt::Ghost& ghost : view_.ghosts()) DrawGhost(sprites_, ghost, view_.side());
    for (const tt::WeaponState& weapon : view_.weapons()) DrawWeapon(sprites_, weapon);
    for (const tt::ViewProjectile& p : view_.projectiles()) DrawProjectile(sprites_, view_.ProjectilePos(p), p.vel);
    EndMode2D();

    DrawHud();
    deck_.Draw(view_.hand(), drag_slot_);
    if (drag_slot_ >= 0 && view_.hand()[drag_slot_].type) deck_.DrawDragged(*view_.hand()[drag_slot_].type, GetMousePosition());

    if (lost_) {
        DrawOverlay("Connection lost", lost_reason_);
        return;
    }
    if (const auto result = view_.result()) {
        const tt::Side me = view_.side();
        const bool draw = result->winner == tt::Winner::Draw;
        const bool won = !draw && (result->winner == tt::Winner::White) == (me == tt::Side::White);
        const int mine = me == tt::Side::White ? result->white : result->black;
        const int theirs = me == tt::Side::White ? result->black : result->white;
        const char* title = result->reason == tt::EndReason::Cancelled ? "Cancelled" : draw ? "Draw" : won ? "You win!" : "You lose";
        std::string detail = TextFormat("You %d  -  %d Them", mine, theirs);
        if (result->reason == tt::EndReason::Forfeit) detail = won ? "Your opponent left the match" : "You left the match";
        if (result->reason == tt::EndReason::Cancelled) detail = "A player left before the start";
        DrawOverlay(title, detail);
        return;
    }

    if (view_.phase() == tt::MatchView::Phase::Countdown) {
        DrawRectangle(0, 300, kScreenW, 190, Color{0, 0, 0, 140});
        DrawCentered(TextFormat("%d", (int)ceilf(view_.CountdownLeft())), 315, 110, RAYWHITE);
        DrawCentered(view_.side() == tt::Side::White ? "You are White" : "You are Black", 435, 30, RAYWHITE);
    }
    if (now_ < toast_until_ && !toast_.empty()) {
        const int width = MeasureText(toast_.c_str(), 20) + 24;
        DrawRectangle((kScreenW - width) / 2, kHudY - 48, width, 34, Color{0, 0, 0, 170});
        DrawCentered(toast_.c_str(), kHudY - 41, 20, RAYWHITE);
    }
}

void MatchScreen::DrawHud() const {
    DrawRectangle(0, kHudY, kScreenW, kHudH, Color{35, 35, 35, 255});
    const tt::Side me = view_.side(), them = tt::Opponent(me);
    const int mine = view_.grid().Count(me), theirs = view_.grid().Count(them);

    DrawSwatch(10, kHudY + 10, me);
    DrawText(TextFormat("You %d", mine), 32, kHudY + 8, 20, RAYWHITE);
    const char* right = TextFormat("%d Them", theirs);
    const int right_w = MeasureText(right, 20);
    DrawText(right, kScreenW - 32 - right_w, kHudY + 8, 20, RAYWHITE);
    DrawSwatch(kScreenW - 24, kHudY + 10, them);

    const int seconds = (int)ceilf(view_.SecondsLeft() - view_.CountdownLeft());
    DrawCentered(TextFormat("%d:%02d", seconds / 60, seconds % 60), kHudY + 6, 22, RAYWHITE);
    if (rtt_ms_ > 0) DrawText(TextFormat("%d ms", rtt_ms_), kScreenW / 2 + 40, kHudY + 13, 10, GRAY);

    // territory bar, white share from the left
    const int white_w = kScreenW * view_.grid().Count(tt::Side::White) / (tt::kCols * tt::kRows);
    DrawRectangle(0, kHudY + kHudH - 5, white_w, 5, kWhiteTile);
    DrawRectangle(white_w, kHudY + kHudH - 5, kScreenW - white_w, 5, Color{90, 90, 90, 255});
}

void MatchScreen::DrawOverlay(const char* title, const std::string& detail) const {
    DrawRectangle(0, 0, kScreenW, kHudY, Color{0, 0, 0, 170});
    DrawCentered(title, 300, 56, RAYWHITE);
    DrawCentered(detail.c_str(), 380, 22, LIGHTGRAY);
    const Rectangle button = MenuButton();
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), button);
    DrawRectangleRounded(button, 0.3f, 16, hovered ? Color{39, 174, 96, 255} : Color{46, 204, 113, 255});
    DrawText("Back to menu", (int)(button.x + (button.width - MeasureText("Back to menu", 22)) / 2),
             (int)(button.y + (button.height - 22) / 2), 22, RAYWHITE);
}
