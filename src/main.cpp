#include "raylib.h"
#include <vector>
#include <iostream>

struct Projectile {
    Color color;
    float x;
    float y;
    float radius;
    float speed;
    float angle;
};


struct GameObject {
    public:
        virtual void update() = 0;
        virtual void draw() = 0;
        virtual ~GameObject() = default;
};

struct WeaponCard : public GameObject {
    public:
        WeaponCard(Texture2D& sprite_sheet, float sheet_size, float width, float height):
            frameWidth(width),
            frameHeight(height),
            sprite_sheet(sprite_sheet),
            frame((Rectangle){150, 0, width, height}){}
        void setxy(Vector2 pos){position = pos;}
        /*
         * Check if we are being moved around
         */
        void update(){}
        void draw(){
            DrawTextureRec(sprite_sheet, frame, position, WHITE);
        }
    private: 
        Texture2D& sprite_sheet;
        Rectangle frame;
        Vector2 position;
        int frameWidth;
        int frameHeight;
};

uint64_t frame_counter = 0;

struct Weapon : public GameObject {
    public:
        Weapon(Texture2D& sprite_sheet, float width, float height, int fpu, int sprites):
            sprite_sheet(sprite_sheet),
            sprites(sprites),
            animate(true),
            frame_width(width),
            frame_height(height),
            position((Vector2){200, 200}),
            frame((Rectangle){0,0,width,height}),
            frames_per_update(fpu),
            current_frame(0){};
        void update(){
            if (!animate) return;
            frame_counter++;
            if (frame_counter >= frames_per_update){
                frame_counter = 0;
                current_frame++;
                frame.x = frame_width*(current_frame % sprites);
            }
        }
        void draw(){
            DrawTextureRec(sprite_sheet, frame, position, WHITE);
        }
    private: 
        uint64_t frame_counter = 0;
        Texture2D& sprite_sheet;
        Rectangle frame;
        Vector2 position;
        int frame_width;
        int frame_height;
        int frames_per_update;
        int current_frame;
        int sprites;
        bool animate;
};

int main(void)
{
    // Initialization
    std::vector<GameObject*> game_objects;
    //--------------------------------------------------------------------------------------
    const int screenWidth = 520;
    const int screenHeight = 800;
    std::vector<Projectile> projectiles;

    InitWindow(screenWidth, screenHeight, "TurnThem");

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    Texture2D scannon_sprite_sheet = LoadTexture("assets/scannon_sprite_sheet.png");
    Weapon scannon(scannon_sprite_sheet, 50.0f, 65.0f, 9, 3);
    game_objects.push_back(&scannon);
    //--------------------------------------------------------------------------------------

    WeaponCard scannon_card(scannon_sprite_sheet, 150, 90.0f, 120.0f);
    game_objects.push_back(&scannon_card);
    //--------------------------------------------------------------------------------------

    Texture2D deck_sprite_sheet = LoadTexture("assets/deck_sprite_sheet.png");
    float deck_width = 500;
    float deck_height = 150;
    Rectangle deck_frame = {0, 0, deck_width, deck_height};
    Vector2 deck_position = {10.0f, 640.0f};



    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        frame_counter++;
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------
        /*
        frame_counter++;

        if (frame_counter >= frames_per_update){
            frame_counter = 0;
            current_frame++;
            cannon_frame.x = frameWidth*(current_frame % 3);
        }
        */
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            //DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);
            //DrawTextureRec(cannon_sprite_sheet, cannon_frame, position, WHITE);
            DrawTextureRec(deck_sprite_sheet, deck_frame, deck_position, WHITE);
            for (auto * obj : game_objects) {
                obj->update();
                obj->draw();
            }

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
