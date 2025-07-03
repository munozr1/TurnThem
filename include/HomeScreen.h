#pragma once
#include "raylib.h"

enum class HomeScreenState {
    MENU,
    FINDING_MATCH,
    MATCH_FOUND
};

class HomeScreen {
private:
    Font title_font;
    Font button_font;
    bool font_loaded;
    int curr_count;
    
    // screen state
    HomeScreenState current_state;
    
    // button properties
    Rectangle find_match_button;
    bool is_button_hovered;
    bool is_button_pressed;
    
    // animation properties
    float animation_timer;
    int dot_count;
    
    // colors
    Color background_color;
    Color title_color;
    Color button_color;
    Color button_hover_color;
    Color button_text_color;
    Color finding_text_color;
    
    void InitializeFonts();
    void DrawRoundedButton(Rectangle rect, float roundness, Color color, const char* text, Font font, Color text_color);
    bool IsButtonClicked(Rectangle button, Vector2 mouse_pos, bool mouse_pressed);
    void UpdateFindingMatchAnimation();
    void DrawFindingMatchScreen();

public:
    HomeScreen();
    ~HomeScreen();
    
    void Update();
    void Draw();
    void RunHomeScreen(); // main loop function
};
