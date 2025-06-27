#ifndef SPRITES_H
#define SPRITES_H

#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct SpriteDetails {
    Vector2 pos; // Optional, can be used for layout
    Vector2 dim; // Optional, can be used for scaling
    Rectangle source_rectangle;
};

struct CardData {
    std::string card_image;
    std::vector<std::string> animation_frames;
    std::string silohett_image;
};

class Atlas {
public:
    Atlas(std::filesystem::path sprite_sheet_path, std::filesystem::path sprite_data_file_path);
    
    Texture2D& GetSpriteSheet();
    SpriteDetails GetSprite(const std::string& sprite_name);
    CardData GetCard(const std::string& card_name);

private:
    Texture2D sprite_sheet;
    std::unordered_map<std::string, SpriteDetails> sprite_details;
    std::unordered_map<std::string, CardData> card_data;
    json sprite_data;
};

#endif

