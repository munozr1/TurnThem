#include "weapon.h"
#include "weapon_card.h"
#include "raylib.h"
#include <vector>
#include <random>
#include "GameObject.h"
#include "projectile.h"
#include "weapon_card.h"
#include "sprites.h"
Atlas* sprite_manager = nullptr;
void fire(Texture2D* sprite, Vector2 pos, float speed, float angle);
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

WeaponCard* selected_card;
std::vector<std::string> weapon_card_names = {"cannon","scannon"};
struct Deck : public GameObject{
    public:
        Deck(SpriteDetails deck_sprite_details): cards({nullptr, nullptr, nullptr, nullptr}),
            deck_sprite_details(deck_sprite_details),
            deck_frame((Rectangle){0,0, 500, 150}),
            position((Vector2){10.0f, 640.0f}){
                for (int i = 0; i < 4; ++i) {
                    auto idx = RandomInRange(0, weapon_card_names.size()-1);
                    auto pos = slot_positions[i];
                    auto new_card = GenerateWeaponCard(weapon_card_names[idx]);
                    
                    new_card->setSlotId(i);
                    new_card->setxyDrop(pos);
                    cards[i] = new_card;
                }
            };
        ~Deck(){
            for(auto* card: cards){
                delete card;
            }
        }

        void update() override{
            for (int i = 0; i < 4; ++i) {
                if(cards[i] == nullptr){
                    auto idx = RandomInRange(0, weapon_card_names.size()-1);
                    auto pos = slot_positions[i];
                    auto new_card = GenerateWeaponCard(weapon_card_names[idx]);
                    new_card->setSlotId(i);
                    new_card->setxyDrop(pos);
                    cards[i] = new_card;
                }
            }
        };
        void draw() override {
            DrawTextureRec(sprite_manager->GetSpriteSheet(), deck_sprite_details.source_rectangle, position, WHITE);
            for (int i = 0; i < 4; ++i) {
                if (cards[i]) {
                    cards[i]->draw();
                }
            }
        }


        bool isPointInside(Vector2 point){
            return point.x >= position.x &&
                point.x <= position.x + deck_frame.width &&
                point.y >= position.y &&
                point.y <= position.y + deck_frame.height;
        }
        WeaponCard* getSelectedCard(Vector2 mouse_pos){
            for (auto * card : cards){
                if (card->isPointInside(mouse_pos)) return card;
            }
            return nullptr;
        }
        
        void resetCardPosition(int id){
            if (id > 3 || id < 0) return;
            auto pos = slot_positions[id];
            cards[id]->setxyDrop(pos);
        }
        void removeCard(int slot_id) {
            if (slot_id >= 0 && slot_id < 4) {
                delete cards[slot_id];
                cards[slot_id] = nullptr;
            }
        }



    private:
        SpriteDetails deck_sprite_details;
        Rectangle deck_frame;
        Vector2 position;
        std::array<WeaponCard*, 4> cards;
        std::array<Vector2, 4> slot_positions = {
            Vector2{28, 653},
            Vector2{153, 653},
            Vector2{278, 653},
            Vector2{403, 653}
        };

};



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
    auto cannon_data = sprite_manager->GetCard("cannon");
    auto cannon_details = sprite_manager->GetSprite(cannon_data.silohett_image);
    Vector2 pos = {200,200};
    Weapon* cannon = new Weapon(cannon_details,cannon_data, pos);
    game_objects.push_back(cannon);

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
                    Vector2 mouse_pos = GetMousePosition();
                    selected_card->setxyDrag(mouse_pos);
                    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
                        mouse_state = MouseState::DROP;
                        selected_card->set_dragging(false);
                    }
                    break;
                }
            case MouseState::DROP:{
                    Vector2 mouse_pos = GetMousePosition();
                    mouse_state = MouseState::NORMAL;
                    if(deck->isPointInside(mouse_pos)){
                        deck->resetCardPosition(selected_card->slotId());
                        selected_card->set_dragging(false);
                    }else{
                        PlaceWeapon(selected_card->cardData(), mouse_pos);
                        deck->removeCard(selected_card->slotId());
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
            for(auto* game_object : game_objects){
                game_object->update();
                game_object->draw();
            }
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
    auto new_weapon = new Weapon(details, data, pos);
    game_objects.push_back(new_weapon);
}
