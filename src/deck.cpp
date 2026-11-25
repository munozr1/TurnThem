#include "deck.h"
#include "supply_theme.h"

Deck::Deck(const GameSprites& sprites, float y) : sprites_(sprites), position_{10.0f, y} {}

Rectangle Deck::SlotRect(int slot) const {
    const Rectangle card = sprites_.weapon(tt::WeaponType::Cannon).card;
    return {position_.x + 18 + slot * 125.0f, position_.y + 13, card.width, card.height};
}

bool Deck::Contains(Vector2 point) const {
    const Rectangle deck = sprites_.deck();
    return CheckCollisionPointRec(point, {position_.x, position_.y, deck.width, deck.height});
}

int Deck::SlotAt(Vector2 point, const Hand& hand) const {
    for (int i = 0; i < tt::kHandSize; i++) {
        if (hand[i].type && !hand[i].pending && CheckCollisionPointRec(point, SlotRect(i))) return i;
    }
    return -1;
}

void Deck::Draw(const Hand& hand, int dragging, tt::ElixirPoints available, bool setup) const {
    using namespace supply_ui;
    const Rectangle deck = sprites_.deck();
    const Rectangle tray = {position_.x, position_.y, deck.width, deck.height};
    DrawRectangleRec({0, position_.y, (float)kScreenW, deck.height}, kSteel);
    DrawRectangleRec(tray, kShadow);
    DrawRectangleLinesEx(tray, 2, kEdge);
    // Riveted steel surrounds the same four deployment slots and hitboxes.
    for (float x : {tray.x + 6, tray.x + tray.width - 6}) {
        for (float y : {tray.y + 6, tray.y + tray.height - 6}) {
            DrawCircleV({x, y}, 2, kKhaki);
        }
    }
    for (int i = 0; i < tt::kHandSize; i++) {
        const Rectangle slot = SlotRect(i);
        DrawRectangleRec(slot, kSteel);
        DrawRectangleLinesEx(slot, 2, kEdge);
        if (!hand[i].type || i == dragging) continue;
        // a spent card stays dimmed until the server deals its replacement; an
        // unaffordable one until the supplies arrive
        const bool affordable = available >= tt::CardCostPoints(*hand[i].type);
        const bool infantry = *hand[i].type == tt::WeaponType::Infantry;
        const bool locked = setup && infantry;
        const bool ready = affordable && !hand[i].pending && !locked;
        DrawRectangleRec({slot.x + 3, slot.y + 3, slot.width - 6, slot.height - 6}, ready ? kOlive : kSteel);
        // Reuse the atlas silhouette on a field-equipment plate.
        if (infantry) {
            DrawSquadIcon({slot.x + slot.width / 2, slot.y + 61}, tt::Side::White, ready ? 1.0f : 0.45f);
            DrawText("3 RIFLEMEN", (int)(slot.x + (slot.width - MeasureText("3 RIFLEMEN", 10)) / 2),
                     (int)slot.y + 85, 10, kMuted);
        } else {
            const Rectangle frame = sprites_.weapon(*hand[i].type).frames[0];
            const Vector2 pos = {slot.x + (slot.width - frame.width) / 2, slot.y + 30};
            DrawTextureRec(sprites_.texture(), frame, pos, ready ? kInk : Color{125, 133, 120, 170});
        }

        const Rectangle badge = {slot.x + 5, slot.y + 5, 42, 22};
        DrawRectangleRec(badge, ready ? kKhaki : kMuted);
        DrawCrate({badge.x + 4, badge.y + 5, 15, 12}, kOlive);
        const char* cost = TextFormat("%d", tt::CardCost(*hand[i].type));
        DrawText(cost, (int)badge.x + 25, (int)badge.y + 3, 16, kShadow);
        const char* label = hand[i].pending ? "PENDING" : locked ? "IN BATTLE" :
                            !affordable ? (setup ? "NO ALLOWANCE" : "RESUPPLY") :
                            infantry ? "RIFLE SQUAD" :
                            *hand[i].type == tt::WeaponType::Cannon ? "CANNON" : "S-CANNON";
        DrawRectangleRec({slot.x + 3, slot.y + slot.height - 23, slot.width - 6, 20}, kShadow);
        const int label_size = 12;
        DrawText(label, (int)(slot.x + (slot.width - MeasureText(label, label_size)) / 2),
                 (int)(slot.y + slot.height - 19), label_size, ready ? kInk : kMuted);
    }
}

void Deck::DrawDragged(tt::WeaponType type, Vector2 mouse) const {
    if (type == tt::WeaponType::Infantry) {
        DrawSquadIcon(mouse, tt::Side::White, 0.6f);
        return;
    }
    const Rectangle frame = sprites_.weapon(type).frames[0];
    const Vector2 pos = {mouse.x - frame.width / 2.0f, mouse.y - frame.height / 2.0f};
    DrawTextureRec(sprites_.texture(), frame, pos, Color{255, 255, 255, 128});
}
