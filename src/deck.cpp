#include "deck.h"

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

void Deck::Draw(const Hand& hand, int dragging) const {
    DrawTextureRec(sprites_.texture(), sprites_.deck(), position_, WHITE);
    for (int i = 0; i < tt::kHandSize; i++) {
        if (!hand[i].type || i == dragging) continue;
        const Rectangle slot = SlotRect(i);
        // a spent card stays dimmed until the server deals its replacement
        const Color tint = hand[i].pending ? Color{255, 255, 255, 90} : WHITE;
        DrawTextureRec(sprites_.texture(), sprites_.weapon(*hand[i].type).card, {slot.x, slot.y}, tint);
    }
}

void Deck::DrawDragged(tt::WeaponType type, Vector2 mouse) const {
    const Rectangle frame = sprites_.weapon(type).frames[0];
    const Vector2 pos = {mouse.x - frame.width / 2.0f, mouse.y - frame.height / 2.0f};
    DrawTextureRec(sprites_.texture(), frame, pos, Color{255, 255, 255, 128});
}
