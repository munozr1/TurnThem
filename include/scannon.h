#ifndef SCANNON_H
#define SCANNON_H

#include "raylib.h"
#include "GameObject.h"
#include "sprites.h"
#include <cstdint>

extern Atlas* sprite_manager;
extern void fire(SpriteDetails sprite, Vector2 pos, float speed, float angle);

enum RotationDirection {
    CW,
    CCW
};

struct SCannon: public GameObject {
    public:
        SCannon(CardData data, Vector2 pos);
        void update();
        void draw();
        CardData cardData();
    private: 
        CardData card_data;
        Rectangle frame;
        Vector2 position;
        uint64_t frame_counter = 0;
        int frames_per_update;
        int current_frame;
        bool animate;
        
        // Rotation specific properties
        float angle_deg = 0;
        RotationDirection current_rotation_direction = CW;
};

#endif
