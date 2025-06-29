#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "raylib.h"
#include "GameObject.h"
#include "sprites.h"

extern Atlas* sprite_manager;
struct Projectile : public GameObject{
    public:
        Projectile(SpriteDetails sprite_details, Vector2 pos, float speed, float angle);
        void update();
        void draw();       
        bool isOutOfBounds(int screenWidth, int screenHeight);
    private:
        SpriteDetails sprite_details;
        Vector2 position;
        float speed;
        float angle_deg;
};

#endif


