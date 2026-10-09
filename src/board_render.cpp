#include "board_render.h"
#include <math.h>
#include "tt/weapons.h"

GameSprites::GameSprites(Atlas& atlas) : atlas_(atlas) {
    for (int i = 0; i < (int)tt::WeaponType::Count; i++) {
        const CardData card = atlas.GetCard(CardNameFor((tt::WeaponType)i));
        WeaponSprites& sprites = weapons_[i];
        for (int f = 0; f < tt::kAnimFrames && f < (int)card.animation_frames.size(); f++) {
            sprites.frames[f] = atlas.GetSprite(card.animation_frames[f]).source_rectangle;
        }
        sprites.card = atlas.GetSprite(card.card_image).source_rectangle;
    }
    shell_ = atlas.GetSprite("shell_projectile.png").source_rectangle;
    deck_ = atlas.GetSprite("Deck.png").source_rectangle;
}

const char* CardNameFor(tt::WeaponType type) {
    switch (type) {
        case tt::WeaponType::Cannon: return "cannon";
        case tt::WeaponType::SCannon: return "scannon";
        case tt::WeaponType::Count: break;
    }
    return "cannon";
}

void DrawBoard(const tt::Grid& grid) {
    for (int row = 0; row < tt::kRows; row++) {
        for (int col = 0; col < tt::kCols; col++) {
            const Rectangle rect = {(float)(col * tt::kCell), (float)(row * tt::kCell), (float)tt::kCell, (float)tt::kCell};
            const Color color = grid.at(col, row).side == tt::Side::White ? Color{240, 240, 240, 255} : Color{60, 60, 60, 255};
            DrawRectangleRec(rect, color);
            DrawRectangleLinesEx(rect, 1.0f, GRAY);
        }
    }
}

void DrawHover(int col, int row, bool valid) {
    const Rectangle rect = {(float)(col * tt::kCell), (float)(row * tt::kCell), (float)tt::kCell, (float)tt::kCell};
    DrawRectangleRec(rect, valid ? Color{0, 255, 0, 200} : Color{255, 0, 0, 200});
}

void DrawWeapon(const GameSprites& sprites, const tt::WeaponState& weapon) {
    const Rectangle frame = sprites.weapon(weapon.type).frames[weapon.anim_frame % tt::kAnimFrames];
    const tt::Vec2 center = tt::CellCenter(weapon.col, weapon.row);
    const Rectangle dest = {center.x, center.y, frame.width, frame.height};
    const Vector2 origin = {frame.width / 2.0f, frame.height / 2.0f};
    DrawTexturePro(sprites.texture(), frame, dest, origin, tt::SpriteRotationDeg(weapon), WHITE);
}

void DrawGhost(const GameSprites& sprites, const tt::Ghost& ghost, tt::Side side) {
    const Rectangle frame = sprites.weapon(ghost.type).frames[0];
    const tt::Vec2 center = tt::CellCenter(ghost.col, ghost.row);
    const Rectangle dest = {center.x, center.y, frame.width, frame.height};
    const Vector2 origin = {frame.width / 2.0f, frame.height / 2.0f};
    DrawTexturePro(sprites.texture(), frame, dest, origin, side == tt::Side::White ? 0.0f : 180.0f, Color{255, 255, 255, 128});
}

void DrawProjectile(const GameSprites& sprites, tt::Vec2 pos, tt::Vec2 vel) {
    const Rectangle frame = sprites.shell();
    const Rectangle dest = {pos.x, pos.y, frame.width, frame.height};
    const Vector2 origin = {frame.width / 2.0f, 6.5f}; // 6.5 is the bullet minus the swoosh
    const float angle = atan2f(vel.y, vel.x) * RAD2DEG;
    DrawTexturePro(sprites.texture(), frame, dest, origin, angle + 90, WHITE);
}
