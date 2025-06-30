#ifndef DECK_H
#define DECK_H

#include "raylib.h"
#include "GameObject.h"
#include "sprites.h"
#include "weapon_card.h"
#include <array>

extern Atlas* sprite_manager;
extern WeaponCard* GenerateWeaponCard(std::string name);
extern std::vector<std::string> weapon_card_names;
extern int RandomInRange(int min, int max);

struct Deck : public GameObject {
    public:
        Deck(SpriteDetails deck_sprite_details);
        ~Deck();
        void update() override;
        void draw() override;
        bool isPointInside(Vector2 point);
        WeaponCard* getSelectedCard(Vector2 mouse_pos);
        void resetCardPosition(int id);
        void removeCard(int slot_id);

    private:
        SpriteDetails deck_sprite_details;
        Rectangle deck_frame;
        Vector2 position;
        std::array<WeaponCard*, 4> cards;
        std::array<Vector2, 4> slot_positions;
};

#endif
