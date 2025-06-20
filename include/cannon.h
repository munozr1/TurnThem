#ifndef CANNON_H
#define CANNON_H

#include "raylib.h"
#include "GameObject.h"
#include  <cstdint>

struct Cannon : public GameObject {
    public:
        Cannon(Texture2D& sprite_sheet, Texture2D& proj_sprite, float width, float height, int fpu, int sprites, float x, float y);
        void update();
        void draw();
    private: 
        Texture2D& sprite_sheet;
        Texture2D& proj_sprite;
        Rectangle frame;
        Vector2 position;
        uint64_t frame_counter = 0;
        int frame_width;
        int frame_height;
        int frames_per_update;
        int current_frame;
        int sprites;
        bool animate;
        double last_fire_time;
};

#endif
