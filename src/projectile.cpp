#include "raylib.h"
#include "sprites.h"
#include "weapon.h"
#include "projectile.h"
#include <math.h>

Projectile::Projectile(SpriteDetails sprite_details, Vector2 pos, float speed, float angle):
    sprite_details(sprite_details), position(pos), speed(speed), angle_deg(angle){};

void Projectile::update(){
    const float dt = GetFrameTime();
    const float rad = angle_deg * DEG2RAD;
    const float dx = speed * cos(rad);
    const float dy = speed * sin(rad);
    position.x += dx * dt;
    position.y += dy * dt;
}

void Projectile::draw(){
    const Rectangle dest = {position.x, position.y, sprite_details.source_rectangle.width, sprite_details.source_rectangle.height};
    const Vector2 origin = {sprite_details.source_rectangle.width/2.0f, 6.5f};// 6.5 is the bulle minus the swoosh
    DrawTexturePro(sprite_manager->GetSpriteSheet(), sprite_details.source_rectangle,dest,origin, angle_deg + 90, WHITE);
}

/* used to check if projectile is out of screen bounds */
bool Projectile::isOutOfBounds(int screenWidth, int screenHeight) {
    return position.x < -50 || position.x > screenWidth + 50 || 
           position.y < -50 || position.y > screenHeight + 50;
}
