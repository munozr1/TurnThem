#include "raylib.h"
#include "sprites.h"
#include "scannon.h"
#include <math.h>

extern void fire(SpriteDetails sprite, Vector2 pos, float speed, float angle);

SCannon::SCannon(CardData data, Vector2 pos):
    card_data(data),
    animate(true),
    position(pos),
    frames_per_update(10), // TODO: fpu should be in json file
    current_frame(0),
    angle_deg(0),
    current_rotation_direction(CW) {
    
    // Initialize frame to first animation frame
    if (!card_data.animation_frames.empty()) {
        auto first_frame_img = card_data.animation_frames.at(0);
        auto first_frame_details = sprite_manager->GetSprite(first_frame_img);
        frame = first_frame_details.source_rectangle;
    }
}

void SCannon::update() {
    // Handle rotation
    if(current_rotation_direction == CW && angle_deg > -45) {
        angle_deg--;
        if(angle_deg == -45) current_rotation_direction = CCW;
    }

    if(current_rotation_direction == CCW && angle_deg < 45) {
        angle_deg++;
        if(angle_deg == 45) current_rotation_direction = CW;
    }

    if (!animate || card_data.animation_frames.empty()) return;
    
    frame_counter++;
    if (frame_counter >= frames_per_update) {
        frame_counter = 0;
        current_frame++;
        
        int frames = card_data.animation_frames.size();
        if (current_frame >= frames) {
            // Fire projectile when animation completes
            if (!card_data.projectile_image.empty()) {
                SpriteDetails projectile_sprite_details = sprite_manager->GetSprite(card_data.projectile_image);
                Vector2 tip = {position.x, position.y};
                fire(projectile_sprite_details, tip, 500, angle_deg - 90);
            }
            current_frame = 0; // Reset animation
        }
        
        // Update frame rectangle
        auto next_frame_img = card_data.animation_frames.at(current_frame);
        auto next_frame_details = sprite_manager->GetSprite(next_frame_img);
        frame = next_frame_details.source_rectangle;
    }
}

void SCannon::draw() {
    Rectangle dest = {position.x, position.y, frame.width, frame.height};
    Vector2 origin = {frame.width/2.0f, frame.height/2.0f};
    DrawTexturePro(sprite_manager->GetSpriteSheet(), frame, dest, origin, angle_deg, WHITE);
}

CardData SCannon::cardData() {
    return card_data;
}
