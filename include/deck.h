#ifndef DECK_H
#define DECK_H

#include <array>
#include "raylib.h"
#include "board_render.h"

// The player's four cards, below the board. The server deals them; the deck
// only draws whatever the match view says is in each slot.
class Deck {
public:
    using Hand = std::array<tt::HandSlotView, tt::kHandSize>;

    Deck(const GameSprites& sprites, float y);

    bool Contains(Vector2 point) const;
    // slot under `point` holding a card that can be played, or -1
    int SlotAt(Vector2 point, const Hand& hand) const;
    // `dragging` is drawn at the mouse instead (DrawDragged); cards costing more
    // than `available` elixir are dimmed
    void Draw(const Hand& hand, int dragging, tt::ElixirPoints available) const;
    void DrawDragged(tt::WeaponType type, Vector2 mouse) const;

private:
    Rectangle SlotRect(int slot) const;

    const GameSprites& sprites_;
    Vector2 position_;
};

#endif
