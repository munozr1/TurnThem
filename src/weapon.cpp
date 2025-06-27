#include "raylib.h"
#include "sprites.h"
#include "weapon.h"

WeaponCard::WeaponCard(Texture2D& sprite_sheet, CardData data, SpriteDetails sprite, SpriteDetails silohett_details): 
    sprite_sheet(sprite_sheet),
    dragging(false),
    slot_id(-1),
    card_data(data),
    sprite_details(sprite),
    silohett_details(silohett_details){}

void WeaponCard::setxyDrag(Vector2 pos){
    pos.x -= silohett_details.dim.x/ 2.0f;
    pos.y -= silohett_details.dim.y/ 2.0f;
    position = pos;
}
void WeaponCard::setxyDrop(Vector2 pos){
    position = pos;
}

bool WeaponCard::isPointInside(Vector2 point){
    return point.x >= position.x &&
        point.x <= position.x + sprite_details.dim.x &&
        point.y >= position.y &&
        point.y <= position.y + sprite_details.dim.y;
}

void WeaponCard::set_dragging(bool b){
    dragging = b;
};

SpriteDetails& WeaponCard::getSilohett(){return silohett_details;}

Vector2 WeaponCard::getPosition() {
    return position;
}

void WeaponCard::update(){}

int WeaponCard::slotId() {
    return slot_id;
}
void WeaponCard::setSlotId(int id) {
    slot_id = id;
}

void WeaponCard::draw(){
    Color tint = dragging ? (Color){255, 255, 255, 128} : WHITE;
    Rectangle source = dragging ?
        silohett_details.source_rectangle :
        sprite_details.source_rectangle;
    // Then try to draw the texture
    DrawTextureRec(sprite_sheet, source, position, tint);
}
