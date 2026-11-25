#include "board_render.h"
#include "weapon_visual.h"
#include <algorithm>
#include <math.h>
#include "tt/weapons.h"

namespace {

const Color kFriendlyTint = {150, 210, 255, 255};
const Color kEnemyTint = {255, 115, 115, 255};

} // namespace

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

void DrawWeapon(const GameSprites& sprites, const tt::WeaponState& weapon, tt::Side viewer) {
    const WeaponVisual visual = WeaponVisualFor(weapon);
    const Rectangle frame = sprites.weapon(weapon.type).frames[visual.frame];
    const tt::Vec2 center = tt::CellCenter(weapon.col, weapon.row);
    const Rectangle dest = {center.x, center.y, frame.width, frame.height};
    const Vector2 origin = {frame.width / 2.0f, frame.height / 2.0f};
    // Preserve olive paint and steel highlights; a team pip carries the strong
    // blue/red identifier instead of washing the entire weapon in that color.
    Color tint = weapon.side == viewer ? Color{230, 240, 225, 255} : Color{245, 210, 195, 255};
    const float brightness = 0.65f + 0.35f * weapon.health / tt::kWeaponMaxHealth;
    tint.r = (unsigned char)(tint.r * brightness);
    tint.g = (unsigned char)(tint.g * brightness);
    tint.b = (unsigned char)(tint.b * brightness);
    DrawTexturePro(sprites.texture(), frame, dest, origin, tt::SpriteRotationDeg(weapon), tint);

    const tt::Vec2 dir = tt::DirFromDeg(tt::SpriteRotationDeg(weapon) - 90);
    const Vector2 pip = {center.x - dir.x * 18, center.y - dir.y * 18};
    DrawCircleV(pip, 4, Color{24, 29, 27, 255});
    DrawCircleV(pip, 2.5f, weapon.side == viewer ? kFriendlyTint : kEnemyTint);

    if (visual.shot_ticks >= 0 && visual.shot_ticks < 12) {
        const float recoil = visual.frame == 1 ? 7.0f : 3.0f;
        const Vector2 muzzle = {center.x + dir.x * (27 - recoil), center.y + dir.y * (27 - recoil)};
        if (visual.shot_ticks < 3) {
            const float flare = 1.0f - visual.shot_ticks / 3.0f;
            const Vector2 tip = {muzzle.x + dir.x * 12 * flare, muzzle.y + dir.y * 12 * flare};
            const Vector2 left = {muzzle.x - dir.y * 5, muzzle.y + dir.x * 5};
            const Vector2 right = {muzzle.x + dir.y * 5, muzzle.y - dir.x * 5};
            DrawTriangle(tip, right, left, Color{245, 174, 65, 230});
            DrawCircleV(muzzle, 3 * flare, Color{255, 238, 182, 255});
        } else {
            const float drift = (float)(visual.shot_ticks - 3);
            const Vector2 smoke = {muzzle.x + dir.x * drift, muzzle.y + dir.y * drift};
            DrawCircleV(smoke, 2.0f + drift * 0.35f, Fade(Color{145, 143, 123, 255}, (12 - visual.shot_ticks) / 24.0f));
        }
    }
}

void DrawGhost(const GameSprites& sprites, const tt::Ghost& ghost, tt::Side side) {
    const Rectangle frame = sprites.weapon(ghost.type).frames[0];
    const tt::Vec2 center = tt::CellCenter(ghost.col, ghost.row);
    const Rectangle dest = {center.x, center.y, frame.width, frame.height};
    const Vector2 origin = {frame.width / 2.0f, frame.height / 2.0f};
    Color tint = kFriendlyTint;
    tint.a = 128;
    DrawTexturePro(sprites.texture(), frame, dest, origin, side == tt::Side::White ? 0.0f : 180.0f, tint);
}

void DrawWeaponHealth(const tt::WeaponState& weapon, tt::Side viewer, Vector2 center) {
    const float health = std::clamp(weapon.health / (float)tt::kWeaponMaxHealth, 0.0f, 1.0f);
    const Rectangle bar = {std::clamp(center.x - 22.0f, 2.0f, tt::kBoardW - 46.0f),
                           std::max(2.0f, center.y - 51.0f), 44, 8};
    const Color fill = health > 0.66f ? Color{80, 215, 100, 255} :
                       health > 0.33f ? Color{255, 205, 70, 255} : Color{240, 80, 70, 255};
    DrawRectangleRec(bar, Color{25, 25, 30, 255});
    DrawRectangleRec({bar.x + 2, bar.y + 2, (bar.width - 4) * health, bar.height - 4}, fill);
    DrawRectangleLinesEx(bar, 1, weapon.side == viewer ? kFriendlyTint : kEnemyTint);
}

void DrawProjectile(const GameSprites& sprites, tt::Vec2 pos, tt::Vec2 vel) {
    const Rectangle frame = sprites.shell();
    const Rectangle dest = {pos.x, pos.y, frame.width, frame.height};
    const Vector2 origin = {frame.width / 2.0f, 6.5f}; // 6.5 is the bullet minus the swoosh
    const float angle = atan2f(vel.y, vel.x) * RAD2DEG;
    DrawTexturePro(sprites.texture(), frame, dest, origin, angle + 90, WHITE);
}
