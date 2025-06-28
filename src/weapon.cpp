#include "raylib.h"
#include "sprites.h"
#include "weapon.h"

//extern void fire(Texture2D& sprite, Vector2 pos, float speed, float angle);
Weapon::Weapon(SpriteDetails details, CardData data, Vector2 pos):
    sprite_details(details),
    card_data(data),
    animate(true),
    position(pos),
    frames_per_update(10), //TODO: fpu should be in json file
    last_fire_time(0.0),
    current_frame(0){
    //initialize frame to first animation frame
    if (!card_data.animation_frames.empty()) {
        auto first_frame_img = card_data.animation_frames.at(0);
        auto first_frame_details = sprite_manager->GetSprite(first_frame_img);
        frame = first_frame_details.source_rectangle;
    }
    };

void Weapon::update() {
    if(card_data.animation_frames.empty()) return;
    double current_time = GetTime();
    if (current_time - last_fire_time >= 0.5) {
        //fire(proj_sprite, {position.x + (frame_width / 2.0f), position.y}, 800, -90);
        last_fire_time = current_time;
        animate = true;
        current_frame = 0;
        frame_counter = 0;
    }

    if (animate) {
        frame_counter++;
        int frames = card_data.animation_frames.size();
        if (frame_counter >= frames_per_update) {
            frame_counter = 0;
            current_frame++;
            if (current_frame >= frames) {
                animate = false;
                current_frame = 0;
            } //TODO move to next frame
            auto next_frame_img = card_data.animation_frames.at(current_frame);
            auto next_frame_details = sprite_manager->GetSprite(next_frame_img);
            frame = next_frame_details.source_rectangle;
        }
    }
}

void Weapon::draw(){
    DrawTextureRec(sprite_manager->GetSpriteSheet(), frame, position, WHITE);
}
