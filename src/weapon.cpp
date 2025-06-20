#include "raylib.h"
#include "weapon.h"

WeaponCard::WeaponCard(Texture2D& sprite_sheet, float sheet_size, float width, float height, float x, float y, int slot):
    position((Vector2){x, y}),
    frame_width(width),
    frame_height(height),
    slot_id(slot),
    sprite_sheet(sprite_sheet),
    card_frame((Rectangle){150, 0, width, height}),
    silohett_frame((Rectangle){0, 0, 50, 65}){}

void WeaponCard::setxy(Vector2 pos){
    pos.x -= silohett_frame.width/ 2.0f;
    pos.y -= silohett_frame.height/ 2.0f;
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
    Rectangle frame = dragging ? silohett_frame : card_frame;
    DrawTextureRec(sprite_sheet, frame, position, tint);
}

int WeaponCard::slotId() {
    return slot_id;
}
void WeaponCard::setSlotId(int id) {
    slot_id = id;
}
