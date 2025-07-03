#include <ostream>
#include <random>
#include <iostream>
#include "raylib.h"
#include "weapon.h"
#include "cannon.h"
#include "scannon.h"
#include "projectile.h"
#include "common.h"
#include "mouse_handler.h"

// global variable definitions
const int GRID_COLS = 13;
const int GRID_ROWS = 20;
const int CELL_SIZE = 40;

Atlas* sprite_manager = nullptr;
WeaponCard* selected_card = nullptr;
Block* selected_block = nullptr;
std::vector<std::string> weapon_card_names = {"cannon","scannon"};
std::vector<GameObject*> game_objects;
std::vector<Block> blocks;
std::unordered_map<Coordinate, Block*> grid_map;
uint64_t frame_counter = 0;

// block ostream operator
std::ostream& operator<<(std::ostream& os, const Block& obj) {
    os << "Block(" << obj.rectangle.x<< ", " << obj.rectangle.y<< ") Tint: " << obj.tint;
    return os;
}

int RandomInRange(int min, int max) {
    static std::random_device rd; 
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

WeaponCard* GenerateWeaponCard(std::string name){
    WeaponCard* card = nullptr;
    Texture2D& sprite_sheet = sprite_manager->GetSpriteSheet();
    CardData data = sprite_manager->GetCard(name);
    SpriteDetails sprite = sprite_manager->GetSprite(data.card_image);
    SpriteDetails silohette = sprite_manager->GetSprite(data.silohett_image);
    card = new WeaponCard(sprite_sheet, data, sprite, silohette);
    return card;
}

int main(void)
{
    const int screenWidth = 520;
    const int screenHeight = 800;

    std::vector<Projectile> projectiles;
    InitWindow(screenWidth, screenHeight, "TurnThem");
    InitGrid();
    SetTargetFPS(60);
    MouseState mouse_state = MouseState::NORMAL;
    sprite_manager = new Atlas("assets/sprites_texture_file.png","assets/sprites_data_file.json");

    Deck* deck = new Deck(sprite_manager->GetSprite("Deck.png"));
    game_objects.push_back(deck);
    
    // main game loop
    while (!WindowShouldClose())
    {
        HandleMouseInput(mouse_state, deck);

        frame_counter++;
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            DrawGrid();
            // update/draw game objects first
            for(auto* game_object : game_objects){
                if(game_object != nullptr) {
                    game_object->update();
                    game_object->draw();
                }
            }

            // remove projectiles
            auto it = std::remove_if(game_objects.begin(), game_objects.end(),
                [screenWidth, screenHeight](GameObject* obj) {
                    if (obj == nullptr) return true;
                    auto proj = dynamic_cast<Projectile*>(obj);
                    if(proj && proj->isOutOfBounds(screenWidth, screenHeight)){
                        delete proj;
                        return true;
                    }
                    return false;
                });
            game_objects.erase(it, game_objects.end());
        EndDrawing();
    }

    // cleanup
    for(auto* game_object : game_objects){
        delete game_object;
    }
    delete sprite_manager;
    CloseWindow();

    return 0;
}

void PlaceWeapon(CardData data, Vector2 pos){
    auto details = sprite_manager->GetSprite(data.silohett_image);
    
    Coordinate center_coord = NormalizeCoordinate(pos);
    
    // mark center as occupied
    auto center_it = grid_map.find(center_coord);
    if (center_it != grid_map.end()) {
        center_it->second->occupied = true;
    }
    
    // surrounding directions
    int directions[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0},          {1,  0},
        {-1,  1}, {0,  1}, {1,  1}
    };
    
    // mark surrounding as padding
    for (int i = 0; i < 8; i++) {
        Coordinate padding_coord = {
            center_coord.x + directions[i][0],
            center_coord.y + directions[i][1]
        };
        
        if (padding_coord.x >= 0 && padding_coord.x < GRID_COLS && 
            padding_coord.y >= 0 && padding_coord.y < GRID_ROWS) {
            
            auto it = grid_map.find(padding_coord);
            if (it != grid_map.end()) {
                it->second->is_padding = true;
            }
        }
    }
    
    if (data.weapon == "cannon") {
        auto new_cannon = new Cannon(details, data, pos);
        game_objects.push_back(new_cannon);
    } else if (data.weapon == "scanon") {
        auto new_scannon = new SCannon(data, pos);
        game_objects.push_back(new_scannon);
    } else {
        auto new_weapon = new Weapon(details, data, pos);
        game_objects.push_back(new_weapon);
    }
}

void fire(SpriteDetails sprite, Vector2 pos, float speed, float angle){
    auto new_projectile = new Projectile(sprite, pos, speed, angle);
    game_objects.push_back(new_projectile);
}

void InitGrid() {
    blocks.clear();
    grid_map.clear();
    
    blocks.reserve(GRID_ROWS * GRID_COLS);
    
    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            Block block;
            block.rectangle = {
                (float)(col * CELL_SIZE),
                (float)(row * CELL_SIZE),
                (float)CELL_SIZE,
                (float)CELL_SIZE
            };
            block.side = (row >= GRID_ROWS / 2) ? BlockSide::white : BlockSide::black;
            block.tint = BlockTint::none;
            block.occupied = false;
            block.is_padding = false;
            blocks.push_back(block);

            Coordinate coord = {col, row};
            grid_map[coord] = &blocks.back();
        }
    }
}

Coordinate NormalizeCoordinate(Vector2 coord) {
    int gridX = (int)(coord.x / CELL_SIZE);
    int gridY = (int)(coord.y / CELL_SIZE);
    
    gridX = (gridX < 0) ? 0 : (gridX >= GRID_COLS) ? GRID_COLS - 1 : gridX;
    gridY = (gridY < 0) ? 0 : (gridY >= GRID_ROWS) ? GRID_ROWS - 1 : gridY;
    
    return {gridX, gridY};
}

void DrawGrid() {
    for (const auto& block : blocks) {
        Color cellColor = (block.side == BlockSide::white) ? 
            Color{240, 240, 240, 255} : Color{60, 60, 60, 255};
        
        if (block.tint != BlockTint::none) {
            cellColor = block.tint == BlockTint::green ?
                Color{0, 255, 0, 200}:
                Color{255, 0, 0, 200};
        }
        DrawRectangleRec(block.rectangle, cellColor);
        DrawRectangleLinesEx(block.rectangle, 1.0f, GRAY);
    }
}

bool IsValidWeaponPosition(Vector2 desired_pos){
    Coordinate center_coord = NormalizeCoordinate(desired_pos);
    
    auto center_it = grid_map.find(center_coord);
    if (center_it == grid_map.end() || center_it->second->occupied || center_it->second->is_padding) {
        return false;
    }
    
    return true;
}
