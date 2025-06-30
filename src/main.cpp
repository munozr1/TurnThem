#include "weapon.h"
#include "cannon.h"
#include "scannon.h"
#include "weapon_card.h"
#include "raylib.h"
#include <vector>
#include <random>
#include "GameObject.h"
#include "projectile.h"
#include "weapon_card.h"
#include "sprites.h"
#include "deck.h"
Atlas* sprite_manager = nullptr;
void fire(SpriteDetails sprite, Vector2 pos, float speed, float angle);
void PlaceWeapon(CardData data, Vector2 pos);
int RandomInRange(int min, int max) {
    static std::random_device rd;  // Non-deterministic seed
    static std::mt19937 gen(rd()); // Mersenne Twister engine
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
enum MouseState {
    NORMAL,
    DRAGGING,
    DROP
};

WeaponCard* selected_card = nullptr;
std::vector<std::string> weapon_card_names = {"cannon","scannon"};

std::vector<GameObject*> game_objects;

int main(void)
{
    // Initialization
    const int screenWidth = 520;
    const int screenHeight = 800;

    std::vector<Projectile> projectiles;
    InitWindow(screenWidth, screenHeight, "TurnThem");
    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
                                    //
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
                    Vector2 mouse_pos = GetMousePosition();
                    selected_card->setxyDrag(mouse_pos);
                    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
                        mouse_state = MouseState::DROP;
                        selected_card->set_dragging(false);
                    }
                    break;
                }
            case MouseState::DROP:{
                    if (selected_card == nullptr) {
                        mouse_state = MouseState::NORMAL;
                        break;
                    }
                    Vector2 mouse_pos = GetMousePosition();
                    mouse_state = MouseState::NORMAL;
                    if(deck->isPointInside(mouse_pos)){
                        deck->resetCardPosition(selected_card->slotId());
                        selected_card->set_dragging(false);
                    }else{
                        PlaceWeapon(selected_card->cardData(), mouse_pos);
                        deck->removeCard(selected_card->slotId());
                        // selected_card is now deleted by removeCard, don't use it
                    }
                    selected_card = nullptr;
                    break;
                }
            default:
                break;
        }

        //----------------------------------------------------------------------------------
        frame_counter++;
        BeginDrawing();
            ClearBackground(RAYWHITE);
            //update/draw
            for(auto* game_object : game_objects){
                if(game_object != nullptr) {
                    game_object->update();
                    game_object->draw();
                }
            }

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
