#include "raylib.h"
#include "sprites.h"
#include "deck.h"

Deck::Deck(SpriteDetails deck_sprite_details): 
    cards({nullptr, nullptr, nullptr, nullptr}),
    deck_sprite_details(deck_sprite_details),
    deck_frame((Rectangle){0, 0, 500, 150}),
    position((Vector2){10.0f, 640.0f}),
    slot_positions({
        Vector2{28, 653},
        Vector2{153, 653},
        Vector2{278, 653},
        Vector2{403, 653}
    }) {
    for (int i = 0; i < 4; ++i) {
        auto idx = RandomInRange(0, weapon_card_names.size() - 1);
        auto pos = slot_positions[i];
        auto new_card = GenerateWeaponCard(weapon_card_names[idx]);
        
        new_card->setSlotId(i);
        new_card->setxyDrop(pos);
        cards[i] = new_card;
    }
}

Deck::~Deck() {
    for (auto* card : cards) {
        delete card;
    }
}

void Deck::update() {
    for (int i = 0; i < 4; ++i) {
        if (cards[i] == nullptr) {
            auto idx = RandomInRange(0, weapon_card_names.size() - 1);
            auto pos = slot_positions[i];
            auto new_card = GenerateWeaponCard(weapon_card_names[idx]);
            new_card->setSlotId(i);
            new_card->setxyDrop(pos);
            cards[i] = new_card;
        }
    }
}

void Deck::draw() {
    DrawTextureRec(sprite_manager->GetSpriteSheet(), deck_sprite_details.source_rectangle, position, WHITE);
    for (int i = 0; i < 4; ++i) {
        if (cards[i]) {
            cards[i]->draw();
        }
    }
}

bool Deck::isPointInside(Vector2 point) {
    return point.x >= position.x &&
           point.x <= position.x + deck_frame.width &&
           point.y >= position.y &&
           point.y <= position.y + deck_frame.height;
}

WeaponCard* Deck::getSelectedCard(Vector2 mouse_pos) {
    for (auto* card : cards) {
        if (card->isPointInside(mouse_pos)) return card;
    }
    return nullptr;
}

void Deck::resetCardPosition(int id) {
    if (id > 3 || id < 0) return;
    auto pos = slot_positions[id];
    cards[id]->setxyDrop(pos);
}

void Deck::removeCard(int slot_id) {
    if (slot_id >= 0 && slot_id < 4) {
        delete cards[slot_id];
        cards[slot_id] = nullptr;
    }
}



