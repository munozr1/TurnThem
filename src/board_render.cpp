#include "board_render.h"
#include "weapon_visual.h"
#include <algorithm>
#include <math.h>
#include "tt/weapons.h"

namespace {

const Color kFriendlyTint = {150, 210, 255, 255};
const Color kEnemyTint = {255, 115, 115, 255};

// One compact top-down rifleman, shared by the field, card and squad preview.
// Local y points toward the aim; the feet alternate only while marching.
void DrawTrooper(Vector2 center, Vector2 direction, float stride, Color team,
                 float opacity, bool firing, bool fallen) {
    const float length = hypotf(direction.x, direction.y);
    if (length > 0.01f) { direction.x /= length; direction.y /= length; }
    else direction = {0, -1};
    if (fallen) direction = {-direction.y, direction.x};
    const auto point = [&](float x, float y) {
        return Vector2{center.x - direction.y * x + direction.x * y,
                       center.y + direction.x * x + direction.y * y};
    };
    const auto ink = [&](Color color) { return Fade(color, opacity); };
    DrawEllipse((int)center.x + 1, (int)center.y + 4, 8, 5, ink({23, 27, 23, 100}));
    DrawLineEx(point(-3, -3), point(-3, -10 + stride), 4, ink({45, 48, 38, 255}));
    DrawLineEx(point(3, -3), point(3, -10 - stride), 4, ink({45, 48, 38, 255}));
    DrawCircleV(point(-3, -10 + stride), 2, ink({28, 31, 27, 255}));
    DrawCircleV(point(3, -10 - stride), 2, ink({28, 31, 27, 255}));
    DrawLineEx(point(0, -5), point(0, 3), 11, ink({66, 76, 48, 255}));
    DrawLineEx(point(-4, -3), point(-4, 3), 2, ink({145, 133, 91, 255}));
    DrawLineEx(point(4, -2), point(4, 7), 3, ink({165, 137, 98, 255}));
    // Wood stock, steel receiver and a long narrow barrel make the role readable.
    DrawLineEx(point(5, -1), point(5, 6), 3, ink({102, 72, 44, 255}));
    DrawLineEx(point(5, 6), point(5, 16), 2, ink({34, 40, 38, 255}));
    DrawCircleV(point(0, 4), 5.5f, ink({31, 38, 28, 255}));
    DrawCircleV(point(0, 5), 4.5f, ink({108, 119, 72, 255}));
    DrawLineEx(point(-4, 7), point(4, 7), 2, ink({149, 155, 103, 255}));
    DrawCircleV(point(0, -4), 2.2f, ink(team));
    if (firing && !fallen) {
        DrawLineEx(point(5, 16), point(5, 23), 3, ink({247, 192, 84, 230}));
        DrawCircleV(point(5, 18), 3, ink({255, 238, 178, 255}));
    }
}

} // namespace

GameSprites::GameSprites(Atlas& atlas) : atlas_(atlas) {
    for (int i = 0; i < (int)tt::WeaponType::Count; i++) {
        // Infantry is drawn from geometry, so it needs no synthetic atlas entry.
        if ((tt::WeaponType)i == tt::WeaponType::Infantry) continue;
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
        case tt::WeaponType::Infantry: return "infantry";
        case tt::WeaponType::Count: break;
    }
    return "cannon";
}

void DrawBoard(const tt::Grid& grid, tt::Side viewer, bool deployment) {
    (void)grid;
    DrawRectangle(0, 0, tt::kBoardW, tt::kBoardH, Color{92, 98, 69, 255});
    for (int row = 0; row < tt::kRows; row++) {
        for (int col = 0; col < tt::kCols; col++) {
            const Rectangle rect = {(float)(col * tt::kCell), (float)(row * tt::kCell), (float)tt::kCell, (float)tt::kCell};
            const int shade = (col * 17 + row * 11) % 9;
            const Color color = {(unsigned char)(91 + shade), (unsigned char)(96 + shade), 69, 255};
            DrawRectangleRec(rect, color);
            if ((col + row * 3) % 7 == 0) {
                DrawLine((int)rect.x + 10, (int)rect.y + 24, (int)rect.x + 17, (int)rect.y + 21, Color{118, 119, 83, 140});
            }
            if (deployment) DrawRectangleLinesEx(rect, 1, Color{183, 184, 139, 50});
        }
    }
    // Fixed centreline and deployment boundary; terrain never changes owner.
    const float mid = tt::kBoardH / 2.0f;
    DrawRectangle(0, (int)mid - 14, tt::kBoardW, 28, Color{130, 119, 86, 110});
    for (int x = 8; x < tt::kBoardW; x += 26) DrawLine(x, (int)mid, x + 12, (int)mid, Color{186, 174, 121, 180});
    if (deployment) {
        const int y = viewer == tt::Side::White ? (int)mid : 0;
        DrawRectangle(0, y, tt::kBoardW, (int)mid, Color{105, 165, 190, 22});
        DrawRectangleLinesEx({2, (float)y + 2, tt::kBoardW - 4.0f, mid - 4}, 2, Color{133, 190, 210, 150});
    }
    DrawRectangleLinesEx({1, 1, tt::kBoardW - 2.0f, tt::kBoardH - 2.0f}, 3, Color{46, 52, 40, 255});
}

void DrawHeadquarters(const tt::HeadquartersState& base, tt::Side viewer) {
    const Vector2 p = ToRay(base.pos);
    const Color team = base.side == viewer ? kFriendlyTint : kEnemyTint;
    const float health = base.health / (float)tt::kBaseMaxHealth;
    DrawRectangleRounded({p.x - 45, p.y - 34, 92, 74}, 0.15f, 3, Color{42, 46, 35, 180});
    for (int i = 0; i < 5; i++) {
        DrawRectangleRounded({p.x - 44 + i * 18, p.y - 38, 17, 10}, 0.3f, 3, Color{140, 129, 90, 255});
        DrawRectangleRounded({p.x - 44 + i * 18, p.y + 29, 17, 10}, 0.3f, 3, Color{140, 129, 90, 255});
    }
    if (base.health == 0) {
        DrawEllipse((int)p.x, (int)p.y, 28, 23, Color{43, 43, 34, 255});
        for (int i = 0; i < 6; i++) {
            DrawRectanglePro({p.x - 20 + (i % 3) * 15.0f, p.y - 11 + (i / 3) * 20.0f, 18, 11},
                             {9, 5}, i * 43.0f, Color{107, 108, 86, 255});
        }
        return;
    }
    DrawRectangleRec({p.x - 25, p.y - 21, 50, 42}, Color{51, 57, 44, 255});
    DrawRectangleRec({p.x - 22, p.y - 18, 44, 34}, Color{(unsigned char)(88 + health * 29), (unsigned char)(93 + health * 29), 80, 255});
    DrawRectangleLinesEx({p.x - 22, p.y - 18, 44, 34}, 2, Color{155, 151, 119, 255});
    DrawRectangleRec({p.x - 16, p.y - 12, 32, 9}, Color{38, 45, 37, 255});
    DrawRectangleRec({p.x - 7, p.y + 8, 14, 13}, Color{43, 47, 39, 255});
    DrawRectangleRec({p.x - 18, p.y + 2, 10, 4}, team);
    DrawLineEx({p.x + 20, p.y - 16}, {p.x + 20, p.y - 41}, 2, Color{43, 47, 39, 255});
    DrawTriangle({p.x + 20, p.y - 41}, {p.x + 20, p.y - 28}, {p.x + 37, p.y - 35}, team);
    if (health < 0.5f) {
        DrawLineEx({p.x - 22, p.y - 19}, {p.x - 8, p.y - 8}, 3, Color{56, 57, 45, 255});
        DrawLineEx({p.x - 8, p.y - 8}, {p.x - 17, p.y + 1}, 2, Color{56, 57, 45, 255});
    }
}

void DrawInfantry(const tt::InfantryState& unit, tt::Vec2 position, tt::Vec2 aim,
                  tt::Side viewer, double tick, float opacity, bool fallen) {
    const bool walking = unit.phase == tt::InfantryPhase::Advancing && !fallen;
    const float stride = walking ? sinf((float)tick * 0.3f + unit.id * 2.0f) * 2 : 0;
    const bool firing = unit.phase == tt::InfantryPhase::Attacking && unit.cooldown > tt::kInfantryAttackTicks - 4;
    DrawTrooper(ToRay(position), {aim.x - position.x, aim.y - position.y}, stride,
                unit.side == viewer ? kFriendlyTint : kEnemyTint, opacity, firing, fallen);
}

void DrawSquadIcon(Vector2 center, tt::Side side, float opacity) {
    const Vector2 forward = {0, side == tt::Side::White ? -1.0f : 1.0f};
    for (int i = -1; i <= 1; i++) {
        DrawTrooper({center.x + i * 13.0f, center.y}, forward, 0, kFriendlyTint, opacity, false, false);
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
    if (ghost.type == tt::WeaponType::Infantry) {
        const Vector2 direction = {0, side == tt::Side::White ? -1.0f : 1.0f};
        for (const auto& pos : tt::SquadPositions(side, ghost.col, ghost.row))
            DrawTrooper(ToRay(pos), direction, 0, kFriendlyTint, 0.55f, false, false);
        return;
    }
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

void DrawInfantryHealth(const tt::InfantryState& unit, tt::Side viewer, Vector2 center) {
    const float health = std::clamp(unit.health / (float)tt::kInfantryMaxHealth, 0.0f, 1.0f);
    const Rectangle bar = {center.x - 7, center.y - 17, 14, 3};
    DrawRectangleRec(bar, Color{24, 29, 25, 235});
    DrawRectangleRec({bar.x, bar.y, bar.width * health, bar.height}, unit.side == viewer ? kFriendlyTint : kEnemyTint);
}

void DrawProjectile(const GameSprites& sprites, tt::Vec2 pos, tt::Vec2 vel) {
    const Rectangle frame = sprites.shell();
    const Rectangle dest = {pos.x, pos.y, frame.width, frame.height};
    const Vector2 origin = {frame.width / 2.0f, 6.5f}; // 6.5 is the bullet minus the swoosh
    const float angle = atan2f(vel.y, vel.x) * RAD2DEG;
    DrawTexturePro(sprites.texture(), frame, dest, origin, angle + 90, WHITE);
}
