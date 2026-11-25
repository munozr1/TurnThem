#include "match_screen.h"
#include "supply_theme.h"
#include <algorithm>
#include <math.h>

namespace {

void DrawCentered(const char* text, int y, int size, Color color) {
    DrawText(text, (kScreenW - MeasureText(text, size)) / 2, y, size, color);
}

} // namespace

MatchScreen::MatchScreen(const GameSprites& sprites, double delay_ticks)
    : sprites_(sprites), view_(delay_ticks), deck_(sprites, (float)kDeckY) {
    camera_.zoom = 1.0f;
}

void MatchScreen::OnMessage(const tt::ServerMsg& msg, double now_s) {
    if (std::holds_alternative<tt::MsgMatchStart>(msg)) fallen_.clear();
    view_.OnMessage(msg, now_s);
}

tt::Vec2 MatchScreen::InfantryAim(const tt::InfantryState& unit) const {
    if (unit.target_kind == tt::CombatTargetKind::Weapon) {
        for (const auto& weapon : view_.weapons())
            if (weapon.id == unit.target_id) return tt::CellCenter(weapon.col, weapon.row);
    } else if (unit.target_kind == tt::CombatTargetKind::Infantry) {
        for (const auto& soldier : view_.infantry())
            if (soldier.id == unit.target_id) return view_.InfantryPos(soldier);
    } else if (unit.target_kind == tt::CombatTargetKind::Headquarters) {
        for (const auto& base : view_.bases())
            if (base.id == unit.target_id) return base.pos;
    }
    return {unit.pos.x, unit.pos.y + (unit.side == tt::Side::White ? -1.0f : 1.0f)};
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
    const auto previous_infantry = view_.infantry();
    view_.Update(now_s);
    for (const auto& unit : previous_infantry) {
        const bool alive = std::any_of(view_.infantry().begin(), view_.infantry().end(),
                                      [&](const auto& other) { return unit.id == other.id; });
        if (!alive) fallen_.push_back({unit, unit.pos, InfantryAim(unit), now_s});
    }
    fallen_.erase(std::remove_if(fallen_.begin(), fallen_.end(),
                                [&](const auto& unit) { return now_s - unit.fallen_at >= 0.65; }), fallen_.end());
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
        // the screen cell, turned around for black (the camera rotates the board 180 degrees)
        hover_col_ = (int)(mouse.x / tt::kCell);
        hover_row_ = (int)(mouse.y / tt::kCell);
        if (view_.side() == tt::Side::Black) {
            hover_col_ = tt::kCols - 1 - hover_col_;
            hover_row_ = tt::kRows - 1 - hover_row_;
        }
        hover_ = true;
        hover_valid_ = view_.CheckPlace((uint8_t)drag_slot_, hover_col_, hover_row_) == tt::PlaceError::Ok;
    }

    // released (checking "not down" also catches a release that happened between frames)
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (hover_ && hover_valid_) {
            if (auto request = view_.RequestPlace((uint8_t)drag_slot_, hover_col_, hover_row_)) link.Send(*request);
        } else if (hover_) {
            toast_ = tt::PlaceErrorText(view_.CheckPlace((uint8_t)drag_slot_, hover_col_, hover_row_));
            toast_until_ = now_s + 1.5;
        }
        drag_slot_ = -1;
        hover_ = false;
    }
}

void MatchScreen::Draw() const {
    const bool setup = view_.phase() == tt::MatchView::Phase::Setup;
    BeginMode2D(camera_);
    DrawBoard(view_.grid(), view_.side(), setup || drag_slot_ >= 0);
    for (const auto& base : view_.bases()) DrawHeadquarters(base, view_.side());
    for (const auto& fallen : fallen_) {
        DrawInfantry(fallen.unit, fallen.position, fallen.aim, view_.side(), view_.playback(),
                     (float)(1 - (now_ - fallen.fallen_at) / 0.65), true);
    }
    if (drag_slot_ >= 0 && hover_) DrawHover(hover_col_, hover_row_, hover_valid_);
    for (const tt::Ghost& ghost : view_.ghosts()) DrawGhost(sprites_, ghost, view_.side());
    for (const tt::WeaponState& weapon : view_.weapons()) DrawWeapon(sprites_, weapon, view_.side());
    for (const auto& unit : view_.infantry())
        DrawInfantry(unit, view_.InfantryPos(unit), InfantryAim(unit), view_.side(), view_.playback());
    if (drag_slot_ >= 0 && hover_ && hover_valid_ && view_.hand()[drag_slot_].type) {
        const tt::Ghost preview{0, 0, (uint8_t)drag_slot_, *view_.hand()[drag_slot_].type,
                                (uint8_t)hover_col_, (uint8_t)hover_row_};
        DrawGhost(sprites_, preview, view_.side());
    }
    for (const tt::ViewProjectile& p : view_.projectiles()) DrawProjectile(sprites_, view_.ProjectilePos(p), p.vel);
    EndMode2D();

    for (const tt::WeaponState& weapon : view_.weapons()) {
        const Vector2 center = GetWorldToScreen2D(ToRay(tt::CellCenter(weapon.col, weapon.row)), camera_);
        DrawWeaponHealth(weapon, view_.side(), center);
    }

    DrawHud();
    for (const auto& unit : view_.infantry())
        DrawInfantryHealth(unit, view_.side(), GetWorldToScreen2D(ToRay(view_.InfantryPos(unit)), camera_));
    deck_.Draw(view_.hand(), drag_slot_, view_.AvailableElixir(), setup);
    DrawSupplies();
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
        const char* title = result->reason == tt::EndReason::Cancelled ? "Cancelled" : draw ? "Draw" : won ? "Victory" : "Defeat";
        std::string detail = TextFormat("Your HQ: %d  |  Enemy HQ: %d", mine, theirs);
        if (result->reason == tt::EndReason::Timer) detail = "Time expired. Both headquarters held.";
        if (result->reason == tt::EndReason::Forfeit) detail = won ? "Your opponent left the match" : "You left the match";
        if (result->reason == tt::EndReason::Cancelled) detail = "A player left before the start";
        DrawOverlay(title, detail);
        return;
    }

    if (setup) {
        DrawRectangle(28, 298, kScreenW - 56, 79, Color{24, 29, 27, 225});
        DrawCentered(TextFormat("PREPARE DEFENSES  %ds", (int)ceilf(view_.CountdownLeft())), 310, 23, supply_ui::kKhaki);
        DrawCentered("Place artillery on the blue half below", 340, 18, supply_ui::kInk);
        DrawCentered("Infantry and resupply begin when battle starts", 361, 13, supply_ui::kMuted);
    }
    if (now_ < toast_until_ && !toast_.empty()) {
        const int width = MeasureText(toast_.c_str(), 20) + 24;
        DrawRectangle((kScreenW - width) / 2, kHudY - 48, width, 34, Color{0, 0, 0, 170});
        DrawCentered(toast_.c_str(), kHudY - 41, 20, RAYWHITE);
    }
}

void MatchScreen::DrawSupplies() const {
    using namespace supply_ui;
    DrawRectangle(0, kSuppliesY, kScreenW, kSuppliesH, kSteel);
    DrawLine(10, kSuppliesY, kScreenW - 10, kSuppliesY, kEdge);
    const tt::ElixirPoints points = view_.AvailableElixir();
    const int whole = points / tt::kElixirTicks;
    const bool setup = view_.phase() == tt::MatchView::Phase::Setup;
    const int capacity = setup ? 6 : tt::kMaxElixir;

    DrawCrate({16, kSuppliesY + 8.0f, 22, 16}, kKhaki);
    DrawText(TextFormat("%s  %d / %d", setup ? "SETUP ALLOWANCE" : "SUPPLIES", whole, capacity), 46, kSuppliesY + 8, 18, kInk);
    const bool doubled = view_.DoubleElixir();
    const char* status = setup ? "NO RESUPPLY" : Over() || view_.phase() != tt::MatchView::Phase::Running ? "STANDBY" :
                         doubled ? "RESUPPLY x2" : points == tt::kMaxElixirPoints ? "DEPOT FULL" : "RESUPPLY";
    DrawText(status, kScreenW - 16 - MeasureText(status, 14), kSuppliesY + 10, 14, doubled ? kKhaki : kMuted);

    // Each crate is one supply. The next crate fills smoothly from the bottom,
    // using the same predicted balance that reserves pending deployments.
    constexpr float gap = 5;
    const float width = (kScreenW - 32.0f - gap * (capacity - 1)) / capacity;
    for (int i = 0; i < capacity; i++) {
        const Rectangle crate = {16 + i * (width + gap), kSuppliesY + 31.0f, width, 19};
        const float fill = std::clamp(points / (float)tt::kElixirTicks - i, 0.0f, 1.0f);
        DrawCrate(crate, doubled ? kKhaki : kOlive, fill);
        DrawRectangleLinesEx(crate, 1, fill >= 1 ? kKhaki : kEdge);
    }
}

void MatchScreen::DrawHud() const {
    DrawRectangle(0, kHudY, kScreenW, kHudH, supply_ui::kSteel);
    const tt::Side me = view_.side(), them = tt::Opponent(me);
    const int mine = view_.bases()[(int)me].health, theirs = view_.bases()[(int)them].health;
    const Color friendly = {150, 210, 255, 255}, enemy = {255, 115, 115, 255};
    DrawText(TextFormat("YOUR HQ  %d", mine), 10, kHudY + 6, 17, friendly);
    const char* right = TextFormat("ENEMY HQ  %d", theirs);
    const int right_w = MeasureText(right, 17);
    DrawText(right, kScreenW - 10 - right_w, kHudY + 6, 17, enemy);

    const int seconds = (int)ceilf(view_.SecondsLeft() - view_.CountdownLeft());
    DrawCentered(TextFormat("%d:%02d", seconds / 60, seconds % 60), kHudY + 4, 20, supply_ui::kInk);
    if (rtt_ms_ > 0) DrawCentered(TextFormat("%d ms", rtt_ms_), kHudY + 27, 10, supply_ui::kMuted);
    DrawRectangle(10, kHudY + 29, 174, 5, supply_ui::kShadow);
    DrawRectangle(10, kHudY + 29, 174 * mine / tt::kBaseMaxHealth, 5, friendly);
    DrawRectangle(kScreenW - 184, kHudY + 29, 174, 5, supply_ui::kShadow);
    DrawRectangle(kScreenW - 184, kHudY + 29, 174 * theirs / tt::kBaseMaxHealth, 5, enemy);
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
