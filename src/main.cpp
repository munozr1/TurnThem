#include "raylib.h"
#include <vector>
#include <random>
#include "GameObject.h"
#include "projectile.h"
#include "weapon.h"
//#include "cannon.h"
//#include "scannon.h"

void fire(Texture2D& sprite, Vector2 pos, float speed, float angle);
int RandomInRange(int min, int max) {
    static std::random_device rd;  // Non-deterministic seed
    static std::mt19937 gen(rd()); // Mersenne Twister engine
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

enum class CardType {
    Cannon,
    SCannon,
    // Add more types here
    COUNT // Always keep last to use for random generation
};


struct WeaponCardSpriteInfo {
    CardType type;
    Texture2D* texture;
};

std::vector<WeaponCardSpriteInfo> weapon_card_sprites;

WeaponCard* GenerateWeaponCard(CardType type, Vector2 pos) {
    Texture2D* sprite;

    // Find the texture for the given type
    for (auto& info : weapon_card_sprites) {
        if (info.type == type) {
            sprite = info.texture;
            break;
        }
    }
    switch (type) {
        case CardType::SCannon:
            return new WeaponCard(*sprite, 150, 90.0f, 120.0f, pos.x, pos.y);
        case CardType::Cannon:
            return new WeaponCard(*sprite, 150, 90.0f, 120.0f, pos.x, pos.y);
        default:
            return new WeaponCard(*sprite, 150, 90.0f, 120.0f, pos.x, pos.y);
    }
}


uint64_t frame_counter = 0;
enum MouseState {
    NORMAL,
    DRAGGING,
    DROP
};

std::vector<GameObject*> heap_objects;
std::vector<GameObject*> stack_objects;
std::vector<WeaponCard*> weapon_cards;

struct Deck : public GameObject{
    public:
        Deck(Texture2D& texture): cards({nullptr, nullptr, nullptr, nullptr}),
            deck_sprite(texture),
            deck_frame((Rectangle){0,0, 500, 150}),
            position((Vector2){10.0f, 640.0f}){
                for (int i = 0; i < 4; ++i) {
                    int idx = RandomInRange(0, weapon_card_sprites.size());
                    Vector2 pos = slot_positions[i];
                    cards[i] = GenerateWeaponCard(weapon_card_sprites[idx].type, pos);
                    weapon_cards.push_back(cards[i]);
                }
            };
        void update() override{
            for (int i = 0; i < 4; ++i) {
                if(cards[i] == nullptr){
                    int idx = RandomInRange(0, weapon_card_sprites.size());
                    Vector2 pos = slot_positions[i];
                    cards[i] = GenerateWeaponCard(weapon_card_sprites[idx].type, pos);
                }
            }
        };
        void draw() override {
            DrawTextureRec(deck_sprite, deck_frame, position, WHITE);
            for (auto* card : cards) {
                if (card) card->draw();
            }
        }

    private:
        Texture2D& deck_sprite;
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

int main(void)
{
    // Initialization
    
    MouseState mouse_state = MouseState::NORMAL;
    WeaponCard* dragging_card;
    //--------------------------------------------------------------------------------------

    const int screenWidth = 520;
    const int screenHeight = 800;
    std::vector<Projectile> projectiles;

    InitWindow(screenWidth, screenHeight, "TurnThem");
    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second

    //--------------------------------------------------------------------------------------
    

    Texture2D cannon_sprite_sheet       = LoadTexture("assets/cannon_sprite_sheet.png");
    Texture2D scannon_sprite_sheet      = LoadTexture("assets/scannon_sprite_sheet.png");
    Texture2D shell_projectile_sprite   = LoadTexture("assets/shell_projectile.png");
    Texture2D deck_sprite_sheet         = LoadTexture("assets/deck_sprite_sheet.png");
    weapon_card_sprites.push_back({CardType::Cannon, &cannon_sprite_sheet});
    weapon_card_sprites.push_back({CardType::SCannon, &scannon_sprite_sheet});

    //--------------------------------------------------------------------------------------

    /* TODO: 
    SCannon scannon(scannon_sprite_sheet, shell_projectile_sprite ,50.0f, 65.0f, 9, 3, 200, 200);
    Cannon cannon(cannon_sprite_sheet, shell_projectile_sprite, 50.0f, 65.0f, 9, 3, 200, 280);
    stack_objects.push_back(&scannon);
    stack_objects.push_back(&cannon);
    */
    
    Deck deck(deck_sprite_sheet);
    stack_objects.push_back(&deck);
    //--------------------------------------------------------------------------------------
    /*WeaponCard scannon_card(scannon_sprite_sheet, 150, 90.0f, 120.0f, 28, 653);
    WeaponCard cannon_card(cannon_sprite_sheet, 150, 90.0f, 120.0f, 153, 653);
    stack_objects.push_back(&scannon_card);
    stack_objects.push_back(&cannon_card);
    weapon_cards.push_back(&scannon_card);
    weapon_cards.push_back(&cannon_card);
    */

    //--------------------------------------------------------------------------------------

   


    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        
        switch (mouse_state) {
            case MouseState::NORMAL:
                if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    /* check if the mouse is hovering over a card */
                    Vector2 mouse_pos = GetMousePosition();
                    for (auto card : weapon_cards){
                        if (card->isPointInside(mouse_pos)){
                            mouse_state = MouseState::DRAGGING;
                            card->set_dragging(true);
                            dragging_card = card;
                            break;
                        }
                    }
                }
                break;
            case MouseState::DRAGGING:{
                Vector2 mouse_pos = GetMousePosition();
                dragging_card->setxy(mouse_pos);
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
                    mouse_state = MouseState::NORMAL;
                    dragging_card->set_dragging(false);
                }
                break;
                }
            case MouseState::DROP:
                /* TODO: handle dropping card */ 
                /* if dropped inside deck, reset original position
                 * if dropped on play area, remove from weapon_cards array and add the weapon 
                 * to the play area.
                 */
            default:
                break;
        }

        //----------------------------------------------------------------------------------
        frame_counter++;
        BeginDrawing();

            ClearBackground(RAYWHITE);

            //DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);
            //DrawTextureRec(cannon_sprite_sheet, cannon_frame, position, WHITE);
            for (auto * obj : stack_objects) {
                obj->update();
                obj->draw();
            }
            for (auto it = heap_objects.begin(); it != heap_objects.end(); ) {
                if (*it == nullptr) {
                    it = heap_objects.erase(it);
                } else {
                    //TODO: this has O(n) deletion, copy n-1 into n and delete n-1.
                    Projectile* p = dynamic_cast<Projectile*>(*it);
                    if (p && p->isOutOfBounds(screenWidth, screenHeight)) {
                        delete p;
                        it = heap_objects.erase(it);
                    } else {
                        p->update();
                        p->draw();
                        ++it;
                    }
                }
            }


        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    for (auto obj : heap_objects) {
        delete obj;
    }
    heap_objects.clear();

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

void fire(Texture2D& sprite, Vector2 pos, float speed, float angle){
    if(sprite.id == 0) return;
    Projectile* shell = new Projectile(sprite, pos, {9,19}, speed, angle );
    heap_objects.push_back(shell);
}
