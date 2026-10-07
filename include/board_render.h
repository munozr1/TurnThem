#pragma once
#include <array>
#include "raylib.h"
#include "sprites.h"
#include "tt/match_view.h"

// screen layout: the board on top, a HUD strip, then the deck
constexpr int kScreenW = tt::kBoardW;          // 520
constexpr int kHudY = tt::kBoardH;             // 800
constexpr int kHudH = 40;
constexpr int kDeckY = kHudY + kHudH;          // 840
constexpr int kDeckH = 150;
constexpr int kScreenH = kDeckY + kDeckH;      // 990

struct WeaponSprites {
    std::array<Rectangle, tt::kAnimFrames> frames; // animation frames; frames[0] is also the silhouette
    Rectangle card;
};

// Source rectangles looked up once from the atlas instead of every frame.
class GameSprites {
public:
    explicit GameSprites(Atlas& atlas);
    Texture2D& texture() const { return atlas_.GetSpriteSheet(); }
    const WeaponSprites& weapon(tt::WeaponType type) const { return weapons_[(int)type]; }
    Rectangle shell() const { return shell_; }
    Rectangle deck() const { return deck_; }

private:
    Atlas& atlas_;
    std::array<WeaponSprites, (size_t)tt::WeaponType::Count> weapons_{};
    Rectangle shell_{};
    Rectangle deck_{};
};

const char* CardNameFor(tt::WeaponType type);
inline Vector2 ToRay(tt::Vec2 v) { return {v.x, v.y}; }

// all in board (world) coordinates; the caller sets up the camera
void DrawBoard(const tt::Grid& grid);
void DrawHover(int col, int row, bool valid);
void DrawWeapon(const GameSprites& sprites, const tt::WeaponState& weapon);
void DrawGhost(const GameSprites& sprites, const tt::Ghost& ghost, tt::Side side);
void DrawProjectile(const GameSprites& sprites, tt::Vec2 pos, tt::Vec2 vel);
