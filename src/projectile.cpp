#include "raylib.h"
#include "projectile.h"
#include <math.h>

Projectile::Projectile(Texture2D& sprite, Vector2 pos, Vector2 frame_dim, float speed, float angle):
    sprite(sprite), position(pos), frame((Rectangle){0,0,frame_dim.x, frame_dim.y}), speed(speed), angle_deg(angle){};

void Projectile::update(){
    const float dt = GetFrameTime();
    const float rad = angle_deg * DEG2RAD;
    const float dx = speed * cos(rad);
    const float dy = speed * sin(rad);
    position.x += dx * dt;
    position.y += dy * dt;
}

void Projectile::draw(){
    const Rectangle dest = {position.x, position.y, frame.width, frame.height};
    const Vector2 origin = {frame.width/2.0f, 6.5f};// 6.5 is the bulle minus the swoosh
    DrawTexturePro(sprite, frame,dest,origin, angle_deg + 90, WHITE);
}

/* used to check if projectile is out of screen bounds */
bool Projectile::isOutOfBounds(int screenWidth, int screenHeight) {
    return position.x < -50 || position.x > screenWidth + 50 || 
           position.y < -50 || position.y > screenHeight + 50;
}
