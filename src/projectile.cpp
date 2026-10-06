#include "raylib.h"
#include "sprites.h"
#include "weapon.h"
#include "projectile.h"
#include "collision.h"
#include <math.h>

Projectile::Projectile(SpriteDetails sprite_details, Vector2 pos, float speed, float angle, BlockSide side):
    sprite_details(sprite_details), position(pos), angle_deg(angle), side(side){
    const float rad = angle_deg * DEG2RAD;
    velocity = {speed * cosf(rad), speed * sinf(rad)};
};

void Projectile::update(){
    if (dead) return;
    const float dt = GetFrameTime();
    const Vector2 prev = position;
    position.x += velocity.x * dt;
    position.y += velocity.y * dt;

    // sweep the whole step, not just the new position, so nothing between frames is skipped
    TileHit hit;
    if (DetectTileCollision(prev, position, side, hit)) {
        hit.block->side = side; // turn it our color
        position = hit.point;
        dead = true;
        return;
    }
    if (isOutOfBounds(GetScreenWidth(), GetScreenHeight())) dead = true;
}

void Projectile::draw(){
    const Rectangle dest = {position.x, position.y, sprite_details.source_rectangle.width, sprite_details.source_rectangle.height};
    const Vector2 origin = {sprite_details.source_rectangle.width/2.0f, 6.5f};// 6.5 is the bulle minus the swoosh
    DrawTexturePro(sprite_manager->GetSpriteSheet(), sprite_details.source_rectangle,dest,origin, angle_deg + 90, WHITE);
}

bool Projectile::isDead() const { return dead; }

/* check if projectile is out of screen bounds */
bool Projectile::isOutOfBounds(int screenWidth, int screenHeight) {
    return position.x < -50 || position.x > screenWidth + 50 || 
           position.y < -50 || position.y > screenHeight + 50;
}
