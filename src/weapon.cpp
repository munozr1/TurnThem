#include "raylib.h"
#include "weapon.h"

WeaponCard::WeaponCard(Texture2D& sprite_sheet, float sheet_size, float width, float height, float x, float y):
    position((Vector2){x, y}),
    frame_width(width),
    frame_height(height),
    sprite_sheet(sprite_sheet),
    frame((Rectangle){150, 0, width, height}){}

void WeaponCard::setxy(Vector2 pos){
    pos.x -= (float)frame_width / 2;
    pos.y -= (float)frame_height/ 2;
    position = pos;
}

bool WeaponCard::isPointInside(Vector2 point){
    return point.x >= position.x &&
        point.x <= position.x + frame_width &&
        point.y >= position.y &&
        point.y <= position.y + frame_height;
}

void WeaponCard::set_dragging(bool b){
    dragging = b;
};

void WeaponCard::update(){}

void WeaponCard::draw(){
    Color tint = dragging ? (Color){255, 255, 255, 128} : WHITE;
    DrawTextureRec(sprite_sheet, frame, position, tint);
}