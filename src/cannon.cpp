#include "raylib.h"
#include "cannon.h"

extern void fire(Texture2D& sprite, Vector2 pos, float speed, float angle);

Cannon::Cannon(Texture2D& sprite_sheet, Texture2D& proj_sprite, float width, float height, int fpu, int sprites, float x, float y):
    sprite_sheet(sprite_sheet),
    proj_sprite(proj_sprite),
    sprites(sprites),
    animate(true),
    frame_width(width),
    frame_height(height),
    position((Vector2){x, y}),
    frame((Rectangle){0,0,width,height}),
    frames_per_update(fpu),
    last_fire_time(0.0),
    current_frame(0){};

void Cannon::update() {
    double current_time = GetTime();
    if (current_time - last_fire_time >= 0.5) {
        fire(proj_sprite, {position.x + (frame_width / 2.0f), position.y}, 800, -90);
        last_fire_time = current_time;
        animate = true;
        current_frame = 0;
        frame.x = 0;
        frame_counter = 0;
    }

    if (animate) {
        frame_counter++;
        if (frame_counter >= frames_per_update) {
            frame_counter = 0;
            current_frame++;
            if (current_frame >= sprites) {
                animate = false;
                current_frame = 0;
                frame.x = 0;
            } else {
                frame.x = frame_width * current_frame;
            }
        }
    }
}

void Cannon::draw(){
    DrawTextureRec(sprite_sheet, frame, position, WHITE);
}
