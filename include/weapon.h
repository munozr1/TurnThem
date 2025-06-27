#ifndef WEAPON_H
#define WEAPON_H

#include "raylib.h"
#include "GameObject.h"
#include "sprites.h"

extern Atlas sprite_manager;

struct WeaponCard : public GameObject {
    public:
        WeaponCard(CardData& data, SpriteDetails& sprite, SpriteDetails& silohett_details); 
        void setxyDrag(Vector2 pos);
        void setxyDrop(Vector2 pos);
        bool isPointInside(Vector2 point);
        void set_dragging(bool b);
        void update();
        void draw();
        SpriteDetails& getSilohett();
        int slotId();
        void setSlotId(int id);
    private: 
        CardData& card_data;
        SpriteDetails& sprite_details;
        SpriteDetails& silohett_details;
        bool dragging;
        int slot_id;
        Vector2 position;
        
};

#endif
