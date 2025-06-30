#include "raylib.h"
#include "sprites.h"
#include "scannon.h"
#include <math.h>

extern void fire(SpriteDetails sprite, Vector2 pos, float speed, float angle);

SCannon::SCannon(CardData data, Vector2 pos):
    card_data(data),
    animate(false),
    position(pos),
    frames_per_update(10), // TODO: fpu should be in json file
    current_frame(0),
    last_fire_time(0.0),
    firing_sequence(false),
    current_target_index(0),
    angle_deg(0.0f),
    rotating_to_target(false),
    shot_delay_timer(0.0) {
    
    // Initialize frame to first animation frame
    if (!card_data.animation_frames.empty()) {
        auto first_frame_img = card_data.animation_frames.at(0);
        auto first_frame_details = sprite_manager->GetSprite(first_frame_img);
        frame = first_frame_details.source_rectangle;
    }
}

void SCannon::update() {
    double current_time = GetTime();
    
    if (!firing_sequence && current_time - last_fire_time >= 1.0) {// TODO get rid of this test delay
        firing_sequence = true;
        current_target_index = 0; //start center
        rotating_to_target = true;
        last_fire_time = current_time;
    }
    
    if (firing_sequence) {
        // Handle delay between shots
        if (shot_delay_timer > 0.0 && current_time - shot_delay_timer >= 0.3) { // 0.3 second delay
            current_target_index = (current_target_index + 1) % 5; // cycle
            rotating_to_target = true;
            shot_delay_timer = 0.0; // reset timer
            
            if (current_target_index == 0) {
                firing_sequence = false;
                last_fire_time = current_time;
            }
        }
        
        // swivel to angle for current shot
        if (rotating_to_target && shot_delay_timer == 0.0) {
            float target_angle = target_angles[current_target_index];
            float angle_diff = target_angle - angle_deg;
            
            if (abs(angle_diff) > 1.0f) {
                angle_deg += (angle_diff > 0) ? 2.0f : -2.0f;
            } else {
                angle_deg = target_angle;
                rotating_to_target = false;
                animate = true; // start animation when we reach target angle
                current_frame = 0;
                frame_counter = 0;
            }
        }
        
        // animation and firing
        if (animate && !card_data.animation_frames.empty()) {
            frame_counter++;
            if (frame_counter >= frames_per_update) {
                frame_counter = 0;
                current_frame++;
                
                int frames = card_data.animation_frames.size();
                if (current_frame >= frames) {
                    // fire projectile when animation completes
                    if (!card_data.projectile_image.empty()) {
                        SpriteDetails projectile_sprite_details = sprite_manager->GetSprite(card_data.projectile_image);
                        
                        // calc barrel position
                        float barrel_length = frame.height * 0.4f; // tweek this value for bullet spawn point
                        float rad = angle_deg * DEG2RAD;
                        Vector2 tip = {
                            position.x + barrel_length * sin(rad),
                            position.y - barrel_length * cos(rad)
                        };
                        
                        fire(projectile_sprite_details, tip, 500, angle_deg - 90);
                    }
                    
                    current_frame = 0;
                    animate = false;
                    shot_delay_timer = current_time;
                }
                
                if (current_frame < card_data.animation_frames.size()) {
                    auto next_frame_img = card_data.animation_frames.at(current_frame);
                    auto next_frame_details = sprite_manager->GetSprite(next_frame_img);
                    frame = next_frame_details.source_rectangle;
                }
            }
        }
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
