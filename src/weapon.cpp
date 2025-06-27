#include "raylib.h"
#include "sprites.h"
#include "weapon.h"

WeaponCard::WeaponCard(CardData& data, SpriteDetails& sprite, SpriteDetails& silohett_details): 
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

void WeaponCard::update(){}

void WeaponCard::draw(){
    Color tint = dragging ? (Color){255, 255, 255, 128} : WHITE;
    Rectangle source = dragging ? 
        silohett_details.source_rectangle : 
        sprite_details.source_rectangle;
    DrawTextureRec(sprite_manager.GetSpriteSheet(), source, position, tint);
}

int WeaponCard::slotId() {
    return slot_id;
}
void WeaponCard::setSlotId(int id) {
    slot_id = id;
}
