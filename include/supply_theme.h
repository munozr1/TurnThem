#pragma once
#include "raylib.h"

// Shared paint and crate mark for the deployment tray and supply readout.
namespace supply_ui {

inline constexpr Color kSteel = {39, 46, 43, 255};
inline constexpr Color kShadow = {24, 29, 27, 255};
inline constexpr Color kEdge = {104, 113, 94, 255};
inline constexpr Color kOlive = {91, 107, 65, 255};
inline constexpr Color kKhaki = {192, 169, 112, 255};
inline constexpr Color kInk = {236, 228, 201, 255};
inline constexpr Color kMuted = {159, 165, 144, 255};

// Boxy silhouette and two packing straps stay readable at small window sizes.
inline void DrawCrate(Rectangle box, Color paint, float fill = 1.0f) {
    DrawRectangleRec(box, kShadow);
    const float height = box.height * fill;
    DrawRectangleRec({box.x, box.y + box.height - height, box.width, height}, paint);
    DrawRectangleLinesEx(box, 1, kShadow);
    const float strap = box.width * 0.15f;
    DrawRectangleRec({box.x + box.width * 0.2f, box.y + 1, strap, box.height - 2}, kShadow);
    DrawRectangleRec({box.x + box.width * 0.65f, box.y + 1, strap, box.height - 2}, kShadow);
    if (fill >= 1) DrawLineEx({box.x + 1, box.y + 2}, {box.x + box.width - 1, box.y + 2}, 1, Fade(kInk, 0.35f));
}

} // namespace supply_ui
