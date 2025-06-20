#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "raylib.h"
#include "GameObject.h"

struct Projectile : public GameObject{
    public:
        Projectile(Texture2D& sprite, Vector2 pos, Vector2 frame_dim, float speed, float angle);
        void update();
        void draw();       
        bool isOutOfBounds(int screenWidth, int screenHeight);
    private:
        Texture2D& sprite;
        Vector2 position;
        Vector2 dim;
        Vector2 frame_dim;
        Rectangle frame;
        float speed;
        float angle_deg;
};

#endif


