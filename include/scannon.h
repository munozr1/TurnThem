#ifndef SCANNON_H
#define SCANNON_H

#include "raylib.h"
#include "GameObject.h"
#include <cstdint>

enum RotationDirection {
    CW,
    CCW
};

struct SCannon: public GameObject {
    public:
        SCannon(Texture2D& sprite_sheet, Texture2D& proj_sprite, float width, float height, int fpu, int sprites, float x, float y);
        void update();
        void draw();
    private: 
        uint64_t frame_counter = 0;
        Texture2D& sprite_sheet;
        Texture2D& proj_sprite;
        Rectangle frame;
        Vector2 position;
        int frame_width;
        int frame_height;
        int frames_per_update;
        int current_frame;
        int sprites;
        float angle_deg= 0;
        RotationDirection current_rotation_direction = CW;
        bool animate;
};

#endif
