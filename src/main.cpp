#include "raylib.h"
#include <vector>
#include <cmath>

void fire(Texture2D& sprite, Vector2 pos, float speed, float angle);

struct GameObject {
    public:
        virtual void update() = 0;
        virtual void draw() = 0;
        virtual ~GameObject() = default;
};

struct Projectile : public GameObject{
    public:
        Projectile(Texture2D& sprite, Vector2 pos, Vector2 frame_dim, float speed, float angle):
            sprite(sprite), position(pos), frame((Rectangle){0,0,frame_dim.x, frame_dim.y}), speed(speed), angle_deg(angle){};
        void update(){
            const float dt = GetFrameTime();
            const float rad = angle_deg * DEG2RAD;
            const float dx = speed * cos(rad);
            const float dy = speed * sin(rad);
            position.x += dx * dt;
            position.y += dy * dt;
        };
        void draw(){
            const Rectangle dest = {position.x, position.y, frame.width, frame.height};
            const Vector2 origin = {frame.width/2.0f, 6.5f};// 6.5 is the bulle minus the swoosh
            DrawTexturePro(sprite, frame,dest,origin, angle_deg + 90, WHITE);
        };
        
        // Check if projectile is out of screen bounds
        bool isOutOfBounds(int screenWidth, int screenHeight) {
            return position.x < -50 || position.x > screenWidth + 50 || 
                   position.y < -50 || position.y > screenHeight + 50;
        }
        
    private:
        Texture2D& sprite;
        Vector2 position;
        Vector2 dim;
        Vector2 frame_dim;
        Rectangle frame;
        float speed;
        float angle_deg;
};


struct WeaponCard : public GameObject {
    public:
        WeaponCard(Texture2D& sprite_sheet, float sheet_size, float width, float height, float x, float y):
            position((Vector2){x, y}),
            frame_width(width),
            frame_height(height),
            sprite_sheet(sprite_sheet),
            frame((Rectangle){150, 0, width, height}){}
        void setxy(Vector2 pos){
            pos.x -= (float)frame_width / 2;
            pos.y -= (float)frame_height/ 2;
            position = pos;
        }
        bool isPointInside(Vector2 point){
            return point.x >= position.x &&
                point.x <= position.x + frame_width &&
                point.y >= position.y &&
                point.y <= position.y + frame_height;
        }
        void set_dragging(bool b){
            dragging = b;
        };
        void update(){}
        void draw(){
            Color tint = dragging ? (Color){255, 255, 255, 128} : WHITE;
            DrawTextureRec(sprite_sheet, frame, position, tint);
        }
    private: 
        Texture2D& sprite_sheet;
        Rectangle frame;
        Vector2 position;
        int frame_width;
        int frame_height;
        bool dragging = false;
};

uint64_t frame_counter = 0;

struct Cannon : public GameObject {
    public:
        Cannon(Texture2D& sprite_sheet, Texture2D& proj_sprite, float width, float height, int fpu, int sprites, float x, float y):
            sprite_sheet(sprite_sheet),
            proj_sprite(proj_sprite),
            sprites(sprites),
            animate(true),
            frame_width(width),
            frame_height(height),
            position((Vector2){x, y}),
            frame((Rectangle){0,0,width,height}),
            frames_per_update(fpu),
            last_fire_time(0.0),
            current_frame(0){};
            void update() {
            double current_time = GetTime();

            // Fire and start animation every 0.5 seconds
            if (current_time - last_fire_time >= 0.5) {
                fire(proj_sprite, {position.x + (frame_width / 2.0f), position.y}, 800, -90);
                last_fire_time = current_time;
                animate = true;
                current_frame = 0;
                frame.x = 0;
                frame_counter = 0;
            }

            if (animate) {
                frame_counter++;
                if (frame_counter >= frames_per_update) {
                    frame_counter = 0;
                    current_frame++;
                    if (current_frame >= sprites) {
                        animate = false;  // stop animating after finishing the cycle
                        current_frame = 0;
                        frame.x = 0;
                    } else {
                        frame.x = frame_width * current_frame;
                    }
                }
            }
        }
        void draw(){
            DrawTextureRec(sprite_sheet, frame, position, WHITE);
        }
    private: 
        Texture2D& sprite_sheet;
        Texture2D& proj_sprite;
        Rectangle frame;
        Vector2 position;
        uint64_t frame_counter = 0;
        int frame_width;
        int frame_height;
        int frames_per_update;
        int current_frame;
        int sprites;
        bool animate;
        double last_fire_time;
};

enum RotationDirection {
    CW,
    CCW
};
struct SCannon: public GameObject {
    public:
        SCannon(Texture2D& sprite_sheet, Texture2D& proj_sprite, float width, float height, int fpu, int sprites, float x, float y):
            sprite_sheet(sprite_sheet),
            proj_sprite(proj_sprite),
            sprites(sprites),
            animate(true),
            frame_width(width),
            frame_height(height),
            position((Vector2){x, y}),
            frame((Rectangle){0,0,width,height}),
            frames_per_update(fpu),
            current_frame(0){};
        /*
         * Swivel Cannon should rotate from 45-135 and complete an animation every 30 degrees.
         */
        void update(){
            if(current_rotation_direction == CW && angle_deg> -45){
                angle_deg--;
                if(angle_deg == -45) current_rotation_direction = CCW;
            }

            if(current_rotation_direction == CCW && angle_deg< 45){
                angle_deg++;
                if(angle_deg == 45) current_rotation_direction = CW;
            }

            if (!animate) return;
            frame_counter++;
            if (!(frame_counter % frames_per_update)){
                frame_counter = 0;
                current_frame++;
                if (current_frame % sprites == 0) {
                    float angle_rad = angle_deg * DEG2RAD;
                    float barrel_length = 19.7f;
                    Vector2 origin = {frame_width / 2.0f, 0};
                    Vector2 tip = {position.x , position.y};
                    fire(proj_sprite, tip, 500, angle_deg - 90);
                }
                frame.x = frame_width*(current_frame % sprites);
            }
        }
        void draw(){
            Rectangle dest = {position.x, position.y, (float)frame_width, (float)frame_height};
            Vector2 origin = {frame_width/2.0f, frame_height/2.0f};
            DrawTexturePro(sprite_sheet, frame, dest, origin, angle_deg, WHITE);
        }
    private: 
        uint64_t frame_counter = 0;
        Texture2D& sprite_sheet;
        Texture2D& proj_sprite;
        Rectangle frame;
        Vector2 position;
        int frame_width;
        int frame_height;
        int frames_per_update;
        int current_frame;
        int sprites;
        float angle_deg= 0;
        RotationDirection current_rotation_direction = CW;
        bool animate;
};
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

    SCannon scannon(scannon_sprite_sheet, shell_projectile_sprite ,50.0f, 65.0f, 9, 3, 200, 200);
    Cannon cannon(cannon_sprite_sheet, shell_projectile_sprite, 50.0f, 65.0f, 9, 3, 200, 280);
    stack_objects.push_back(&scannon);
    stack_objects.push_back(&cannon);
    
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
