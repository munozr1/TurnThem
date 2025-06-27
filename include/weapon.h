#ifndef WEAPON_H
#define WEAPON_H

#include "raylib.h"
#include "GameObject.h"
#include "sprites.h"


struct WeaponCard : public GameObject {
    public:
        WeaponCard(Texture2D& sprite_sheet, CardData& data, SpriteDetails& sprite, SpriteDetails& silohett_details); 
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
        Texture2D& sprite_sheet;
        CardData& card_data;
        SpriteDetails& sprite_details;
        SpriteDetails& silohett_details;
        bool dragging;
        int slot_id;
        Vector2 position;
        
};

#endif
