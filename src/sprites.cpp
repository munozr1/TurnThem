#include "sprites.h"
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <iostream>
Atlas::Atlas(std::filesystem::path sprite_sheet_path, std::filesystem::path sprite_data_file_path) {
    std::ifstream f(sprite_data_file_path);
    sprite_data = json::parse(f);

    sprite_sheet = LoadTexture(sprite_sheet_path.string().c_str());

    // Load frames
    for (auto& [name, entry] : sprite_data["frames"].items()) {
        auto frame = entry["frame"];
        SpriteDetails details;
        details.source_rectangle = {
            static_cast<float>(frame["x"]),
            static_cast<float>(frame["y"]),
            static_cast<float>(frame["w"]),
            static_cast<float>(frame["h"])
        };
        details.pos = { 0, 0 };
        details.dim = {
            static_cast<float>(frame["w"]),
            static_cast<float>(frame["h"])
        };
        sprite_details[name] = details;
    }

    // Load cards
    for (auto& [card_name, card_info] : sprite_data["cards"].items()) {
        CardData data;
        data.card_image = card_info["cardImage"];
        data.silohett_image = card_info[data.animation_frames.at(0)];
        for (auto& frame_name : card_info["frames"]) {
            data.animation_frames.push_back(frame_name.get<std::string>());
        }
        card_data[card_name] = data;
    }
}

Texture2D& Atlas::GetSpriteSheet() {
    return sprite_sheet;
}

SpriteDetails Atlas::GetSprite(const std::string& sprite_name) {
    auto it = sprite_details.find(sprite_name);
    if (it == sprite_details.end()) {
        std::cerr << "Sprite not found: " << sprite_name << std::endl;
        return SpriteDetails{ Vector2{0, 0}, Vector2{0, 0}, Rectangle{0, 0, 0, 0} };
    }
    return it->second;
}

CardData Atlas::GetCard(const std::string& card_name) {
    auto it = card_data.find(card_name);
    if (it == card_data.end()) {
        std::cerr << "Card not found: " << card_name << std::endl;
        return CardData{};
    }
    return it->second;
}

