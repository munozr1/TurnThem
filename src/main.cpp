#include "raylib.h"
#include <vector>
#include <random>
#include "GameObject.h"
#include "projectile.h"
#include "weapon.h"
#include "sprites.h"
enum class CardType {
    Cannon,
    SCannon,
    // Add more types here
    COUNT // Always keep last to use for random generation
};


void fire(Texture2D* sprite, Vector2 pos, float speed, float angle);
void PlaceWeapon(CardType type, Vector2 pos);
int RandomInRange(int min, int max) {
    static std::random_device rd;  // Non-deterministic seed
    static std::mt19937 gen(rd()); // Mersenne Twister engine
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}
uint64_t frame_counter = 0;
enum MouseState {
    NORMAL,
    DRAGGING,
    DROP
};

WeaponCard* selected_card;

int main(void)
{
    // Initialization
    
    //--------------------------------------------------------------------------------------

    const int screenWidth = 520;
    const int screenHeight = 800;
    std::vector<Projectile> projectiles;

    InitWindow(screenWidth, screenHeight, "TurnThem");
    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    Atlas sprite_manager("assets/sprites_texture_file.png","assets/sprites_data_file.json");
    MouseState mouse_state = MouseState::NORMAL;
    std::vector<GameObject> game_objects;

    //--------------------------------------------------------------------------------------
    
    CardData cannon_data = sprite_manager.GetCard("cannon");
    SpriteDetails cannon_details = sprite_manager.GetSprite(cannon_data.card_image);
    SpriteDetails cannon_silohett_details = sprite_manager.GetSprite(cannon_data.silohett_image);
    WeaponCard test_cannon_card(cannon_data, cannon_details, cannon_silohett_details);
    game_objects.push_back(test_cannon_card);


    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        
        //TODO: check mouse state for dragging cards
        switch (mouse_state) {
            case MouseState::NORMAL:
                if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

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
                    /* TODO: determin valid position */
                    Vector2 mouse_pos = GetMousePosition();
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
            for(auto& game_object : game_objects){
                game_object.update();
                game_object.draw();
            }
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
