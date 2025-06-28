#ifndef WEAPON_H
#define WEAPON_H

#include "raylib.h"
#include "GameObject.h"
#include "sprites.h"
#include  <cstdint>

extern Atlas* sprite_manager;

struct Weapon: public GameObject {
    public:
        Weapon(SpriteDetails sprite_details, CardData data, Vector2 pos);
        void update();
        void draw();
    private: 
        CardData card_data;
        SpriteDetails sprite_details;
        Rectangle frame;
        Vector2 position;
        uint64_t frame_counter = 0;
        int frames_per_update;
        int current_frame;
        bool animate;
        double last_fire_time;
};

#endif
