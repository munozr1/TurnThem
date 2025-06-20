#include "raylib.h"
#include <vector>
#include "projectile.h"
#include "weapon.h"
//#include "cannon.h"
//#include "scannon.h"

void fire(Texture2D& sprite, Vector2 pos, float speed, float angle);

uint64_t frame_counter = 0;
enum MouseState {
    NORMAL,
    DRAGGING,
    DROP
};

std::vector<GameObject*> heap_objects;
std::vector<GameObject*> stack_objects;
std::vector<WeaponCard*> weapon_cards;

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

    //--------------------------------------------------------------------------------------

    /* TODO: 
    SCannon scannon(scannon_sprite_sheet, shell_projectile_sprite ,50.0f, 65.0f, 9, 3, 200, 200);
    Cannon cannon(cannon_sprite_sheet, shell_projectile_sprite, 50.0f, 65.0f, 9, 3, 200, 280);
    stack_objects.push_back(&scannon);
    stack_objects.push_back(&cannon);
    */
    
    //--------------------------------------------------------------------------------------
    WeaponCard scannon_card(scannon_sprite_sheet, 150, 90.0f, 120.0f, 28, 653);
    WeaponCard cannon_card(cannon_sprite_sheet, 150, 90.0f, 120.0f, 153, 653);
    stack_objects.push_back(&scannon_card);
    stack_objects.push_back(&cannon_card);
    weapon_cards.push_back(&scannon_card);
    weapon_cards.push_back(&cannon_card);
    //--------------------------------------------------------------------------------------

    float deck_width = 500;
    float deck_height = 150;
    Rectangle deck_frame = {0, 0, deck_width, deck_height};
    Vector2 deck_position = {10.0f, 640.0f};


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
            DrawTextureRec(deck_sprite_sheet, deck_frame, deck_position, WHITE);
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
