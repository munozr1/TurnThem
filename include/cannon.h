#ifndef CANNON_H
#define CANNON_H

#include "raylib.h"
#include "GameObject.h"
#include "sprites.h"
#include "common.h"
#include <cstdint>

extern Atlas* sprite_manager;
extern void fire(SpriteDetails sprite, Vector2 pos, float speed, float angle, BlockSide side);

struct Cannon: public GameObject {
    public:
        Cannon(SpriteDetails sprite_details, CardData data, Vector2 pos, BlockSide side);
        void update();
        void draw();
        CardData cardData();
    private: 
        CardData card_data;
        SpriteDetails sprite_details;
        Rectangle frame;
        Vector2 position;
        BlockSide side; // owner; passed on to every projectile fired
        uint64_t frame_counter = 0;
        int frames_per_update;
        int current_frame;
        bool animate;
        double last_fire_time;
};

#endif
