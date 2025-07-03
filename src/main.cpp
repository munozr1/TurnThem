#include <ostream>
#include <unordered_map>
#include <vector>
#include <random>
#include <iostream>
#include "raylib.h"
#include "weapon.h"
#include "cannon.h"
#include "scannon.h"
#include "weapon_card.h"
#include "GameObject.h"
#include "projectile.h"
#include "weapon_card.h"
#include "sprites.h"
#include "deck.h"
enum MouseState {
    NORMAL,
    DRAGGING,
    DROP
};
enum BlockSide {
    white,
    black 
};
enum BlockTint {
    none,
    green,
    red
};

struct Block{
    Rectangle rectangle;
    BlockSide side;
    BlockTint tint; // if the user is hovering a card over the block (block is selected and used when user drops card)
    Vector2 getPos(){
        return Vector2{rectangle.x, rectangle.y};
    }
    friend std::ostream& operator<<(std::ostream& os, const Block& obj);
};
std::ostream& operator<<(std::ostream& os, const Block& obj) {
    os << "Block(" << obj.rectangle.x<< ", " << obj.rectangle.y<< ") Tint: " << obj.tint;
    return os;
}

struct Coordinate{
    int x;
    int y;
    
    bool operator==(const Coordinate& other) const {
        return x == other.x && y == other.y;
    }
};


Atlas* sprite_manager = nullptr;
void fire(SpriteDetails sprite, Vector2 pos, float speed, float angle);
void PlaceWeapon(CardData data, Vector2 pos);
void InitGrid();
void DrawGrid();
Coordinate NormalizeCoordinate(Vector2 coord);
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
uint64_t frame_counter = 0;
// Hash function for Coordinate to use in unordered_map
namespace std {
    template <>
    struct hash<Coordinate> {
        size_t operator()(const Coordinate& coord) const {
            return hash<int>()(coord.x) ^ (hash<int>()(coord.y) << 1);
        }
    };
}
WeaponCard* selected_card   = nullptr;
Block* selected_block  = nullptr;
std::vector<std::string> weapon_card_names = {"cannon","scannon"};
std::vector<GameObject*> game_objects;
std::vector<Block> blocks;
std::unordered_map<Coordinate, Block*> grid_map;

// Grid constants
const int GRID_COLS = 13;
const int GRID_ROWS = 20;
const int CELL_SIZE = 40;


int main(void)
{
    // Initialization
    const int screenWidth = 520;
    const int screenHeight = 800;

    std::vector<Projectile> projectiles;
    InitWindow(screenWidth, screenHeight, "TurnThem");
    InitGrid();
    SetTargetFPS(60);
    MouseState mouse_state = MouseState::NORMAL;
    sprite_manager = new Atlas("assets/sprites_texture_file.png","assets/sprites_data_file.json");

    //--------------------------------------------------------------------------------------
    
    Deck* deck = new Deck(sprite_manager->GetSprite("Deck.png"));
    game_objects.push_back(deck);
    
        // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        
        switch (mouse_state) {
            case MouseState::NORMAL:
                if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    Vector2 mouse_pos = GetMousePosition();
                    selected_card = deck->getSelectedCard(mouse_pos);
                    if (selected_card != nullptr){ 
                        mouse_state = MouseState::DRAGGING;
                        selected_card->set_dragging(true);
                    }
                }
                break;
            case MouseState::DRAGGING:{
                    if (selected_card == nullptr) {
                        mouse_state = MouseState::NORMAL;
                        break;
                    }

                    if(selected_block != nullptr) selected_block->tint = BlockTint::none;
                    
                    auto mouse_pos = GetMousePosition();
                    selected_card->setxyDrag(mouse_pos);
                    auto normalized_mouse_pos = NormalizeCoordinate(mouse_pos);

                    auto it = grid_map.find(normalized_mouse_pos);
                    selected_block = it->second;
                    if (selected_block != nullptr) selected_block->tint = BlockTint::green;
                    
                    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
                        mouse_state = MouseState::DROP;
                    }
                    break;
                }
            case MouseState::DROP:{
                    if (selected_card == nullptr) {
                        mouse_state = MouseState::NORMAL;
                        break;
                    }

                    if (selected_block != nullptr) selected_block->tint = BlockTint::none;
                    selected_card->set_dragging(false);
                    Vector2 mouse_pos = GetMousePosition();

                    if(deck->isPointInside(mouse_pos)){
                        deck->resetCardPosition(selected_card->slotId());
                    }else{
                        auto pos = selected_block->getPos();
                        PlaceWeapon(selected_card->cardData(), pos);
                        deck->removeCard(selected_card->slotId());
                    }
                    selected_card  = nullptr;
                    selected_block = nullptr;
                    mouse_state = MouseState::NORMAL;
                    break;
                }
            default:
                break;
        }

        //----------------------------------------------------------------------------------
        frame_counter++;
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            DrawGrid();
            //update/draw game objects first
            for(auto* game_object : game_objects){
                if(game_object != nullptr) {
                    game_object->update();
                    game_object->draw();
                }
            }
            
            // Draw the grid on top so tint is visible

            //remove projectiles
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
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    for(auto* game_object : game_objects){
        delete game_object;
    }
    delete sprite_manager;
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

void PlaceWeapon(CardData data, Vector2 pos){
    auto details = sprite_manager->GetSprite(data.silohett_image);
    
    if (data.weapon == "cannon") {
        auto new_cannon = new Cannon(details, data, pos);
        game_objects.push_back(new_cannon);
    } else if (data.weapon == "scanon") {  // Note: matches the typo in JSON
        auto new_scannon = new SCannon(data, pos);
        game_objects.push_back(new_scannon);
    } else {
        // Default to weapon class for unknown types
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
    
    // Reserve capacity to prevent pointer invalidation during push_back
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
            blocks.push_back(block);

            Coordinate coord = {col, row};
            grid_map[coord] = &blocks.back();
        }
    }
}

Coordinate NormalizeCoordinate(Vector2 coord) {
    // Convert pixel coordinates to grid coordinates
    int gridX = (int)(coord.x / CELL_SIZE);
    int gridY = (int)(coord.y / CELL_SIZE);
    
    // Clamp to grid boundaries
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
