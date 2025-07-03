#pragma once
#include <vector>
#include <unordered_map>
#include "raylib.h"
#include "GameObject.h"
#include "weapon_card.h"
#include "sprites.h"
#include "deck.h"

// enums
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

// structs
struct Block{
    Rectangle rectangle;
    BlockSide side;
    BlockTint tint; // if the user is hovering a card over the block (block is selected and used when user drops card)
    Vector2 getPos(){
        return Vector2{rectangle.x + rectangle.width/2, rectangle.y + rectangle.height/2};
    }
    friend std::ostream& operator<<(std::ostream& os, const Block& obj);
    bool occupied;    // weapon placed here
    bool is_padding;  // clearance around weapon
};

struct Coordinate{
    int x;
    int y;
    
    bool operator==(const Coordinate& other) const {
        return x == other.x && y == other.y;
    }
};

// hash function for Coordinate to use in unordered_map
namespace std {
    template <>
    struct hash<Coordinate> {
        size_t operator()(const Coordinate& coord) const {
            return hash<int>()(coord.x) ^ (hash<int>()(coord.y) << 1);
        }
    };
}

// grid constants
extern const int GRID_COLS;
extern const int GRID_ROWS;
extern const int CELL_SIZE;

// global variables
extern Atlas* sprite_manager;
extern WeaponCard* selected_card;
extern Block* selected_block;
extern std::vector<std::string> weapon_card_names;
extern std::vector<GameObject*> game_objects;
extern std::vector<Block> blocks;
extern std::unordered_map<Coordinate, Block*> grid_map;
extern uint64_t frame_counter;

// function declarations
void fire(SpriteDetails sprite, Vector2 pos, float speed, float angle);
void PlaceWeapon(CardData data, Vector2 pos);
void InitGrid();
void DrawGrid();
Coordinate NormalizeCoordinate(Vector2 coord);
bool IsValidWeaponPosition(Vector2 desired_pos);
WeaponCard* GenerateWeaponCard(std::string name);
int RandomInRange(int min, int max);
