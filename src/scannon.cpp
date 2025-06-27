#include "raylib.h"
#include "scannon.h"

//extern void fire(Texture2D& sprite, Vector2 pos, float speed, float angle);

SCannon::SCannon(Texture2D& sprite_sheet, Texture2D& proj_sprite, float width, float height, int fpu, int sprites, float x, float y):
    sprite_sheet(sprite_sheet),
    proj_sprite(proj_sprite),
    sprites(sprites),
    animate(true),
    frame_width(width),
    frame_height(height),
    position((Vector2){x, y}),
    frame((Rectangle){0,0,width,height}),
    frames_per_update(fpu),
    current_frame(0){};

void SCannon::update(){
    if(current_rotation_direction == CW && angle_deg> -45){
        angle_deg--;
        if(angle_deg == -45) current_rotation_direction = CCW;
    }

    if(current_rotation_direction == CCW && angle_deg< 45){
        angle_deg++;
        if(angle_deg == 45) current_rotation_direction = CW;
    }

    if (!animate) return;
    frame_counter++;
    if (!(frame_counter % frames_per_update)){
        frame_counter = 0;
        current_frame++;
        if (current_frame % sprites == 0) {
            float angle_rad = angle_deg * DEG2RAD;
            float barrel_length = 19.7f;
            Vector2 origin = {frame_width / 2.0f, 0};
            Vector2 tip = {position.x , position.y};
            //fire(proj_sprite, tip, 500, angle_deg - 90);
        }
        frame.x = frame_width*(current_frame % sprites);
    }
}

void SCannon::draw(){
    Rectangle dest = {position.x, position.y, (float)frame_width, (float)frame_height};
    Vector2 origin = {frame_width/2.0f, frame_height/2.0f};
    DrawTexturePro(sprite_sheet, frame, dest, origin, angle_deg, WHITE);
}
