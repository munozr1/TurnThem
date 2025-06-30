#ifndef SCANNON_H
#define SCANNON_H

#include "raylib.h"
#include "GameObject.h"
#include "sprites.h"
#include <cstdint>

extern Atlas* sprite_manager;
extern void fire(SpriteDetails sprite, Vector2 pos, float speed, float angle);

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
        double last_fire_time;
        
        // Continuous firing sequence properties
        bool firing_sequence = false;
        float target_angles[5] = {0.0f, 45.0f, 0.0f, -45.0f, 0.0f}; // center, right, center, left, center
        int current_target_index = 0;
        float angle_deg = 0.0f;
        bool rotating_to_target = false;
        double shot_delay_timer = 0.0;
};

#endif
