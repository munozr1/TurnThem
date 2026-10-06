#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "raylib.h"
#include "GameObject.h"
#include "sprites.h"
#include "common.h"

extern Atlas* sprite_manager;
struct Projectile : public GameObject{
    public:
        Projectile(SpriteDetails sprite_details, Vector2 pos, float speed, float angle, BlockSide side);
        void update() override;
        void draw() override;
        bool isDead() const override;
        bool isOutOfBounds(int screenWidth, int screenHeight);
    private:
        SpriteDetails sprite_details;
        Vector2 position;
        Vector2 velocity; // px/s, worked out once from speed + angle so update() needs no trig
        float angle_deg;
        BlockSide side;   // owner's color; tiles of the other color are hit and converted
        bool dead = false;
};

#endif

