#ifndef WEAPON_H
#define WEAPON_H

#include "raylib.h"
#include "GameObject.h"

struct WeaponCard : public GameObject {
    public:
        WeaponCard(Texture2D& sprite_sheet, float sheet_size, float width, float height, float x, float y);
        void setxy(Vector2 pos);
        bool isPointInside(Vector2 point);
        void set_dragging(bool b);
        void update();
        void draw();
    private: 
        Texture2D& sprite_sheet;
        Rectangle card_frame;
        Rectangle silohett_frame;
        Vector2 position;
        int frame_width;
        int frame_height;
        bool dragging = false;
};

#endif
