#include "HomeScreen.h"
#include <string>
#include <cstdio>

HomeScreen::HomeScreen() {
    current_state = HomeScreenState::MENU;
    font_loaded = false;
    is_button_hovered = false;
    is_button_pressed = false;
    animation_timer = 0.0f;
    dot_count = 0;
    curr_count = 0;
    
    // Initialize colors (Clash Royale inspired)
    background_color = {41, 128, 185, 255}; // Blue background
    title_color = {255, 255, 255, 255}; // White title
    button_color = {46, 204, 113, 255}; // Green button
    button_hover_color = {39, 174, 96, 255}; // Darker green on hover
    button_text_color = {255, 255, 255, 255}; // White button text
    finding_text_color = {255, 255, 255, 255}; // White finding text
    
    InitializeFonts();
    
    // Setup button dimensions and position
    float button_width = 200.0f;
    float button_height = 60.0f;
    find_match_button = {
        (GetScreenWidth() - button_width) / 2.0f,
        GetScreenHeight() / 2.0f + 50.0f,
        button_width,
        button_height
    };
}

HomeScreen::~HomeScreen() {
    if (font_loaded) {
        UnloadFont(title_font);
        UnloadFont(button_font);
    }
}

void HomeScreen::InitializeFonts() {
    // Try to load a custom font, fallback to default if not available
    title_font = LoadFontEx("assets/fonts/title.ttf", 72, 0, 250);
    button_font = LoadFontEx("assets/fonts/button.ttf", 24, 0, 250);
    
    // If custom fonts fail to load, use default font
    if (title_font.texture.id == 0) {
        title_font = GetFontDefault();
    }
    if (button_font.texture.id == 0) {
        button_font = GetFontDefault();
    }
    
    font_loaded = true;
}

void HomeScreen::DrawRoundedButton(Rectangle rect, float roundness, Color color, const char* text, Font font, Color text_color) {
    DrawRectangleRounded(rect, roundness, 16, color);
    
    // Add subtle shadow effect
    Rectangle shadow_rect = {rect.x + 2, rect.y + 2, rect.width, rect.height};
    DrawRectangleRounded(shadow_rect, roundness, 16, {0, 0, 0, 50});
    DrawRectangleRounded(rect, roundness, 16, color);
    
    // Center text in button
    Vector2 text_size = MeasureTextEx(font, text, font.baseSize, 1);
    Vector2 text_pos = {
        rect.x + (rect.width - text_size.x) / 2.0f,
        rect.y + (rect.height - text_size.y) / 2.0f
    };
    
    DrawTextEx(font, text, text_pos, font.baseSize, 1, text_color);
}

bool HomeScreen::IsButtonClicked(Rectangle button, Vector2 mouse_pos, bool mouse_pressed) {
    return CheckCollisionPointRec(mouse_pos, button) && mouse_pressed;
}

void HomeScreen::UpdateFindingMatchAnimation() {
    animation_timer += GetFrameTime();
    
    // Update dots every 0.5 seconds
    if (animation_timer >= 0.5f) {
        dot_count = (dot_count + 1) % 4; // 0, 1, 2, 3 dots
        animation_timer = 0.0f;
    }
}

void HomeScreen::DrawFindingMatchScreen() {
    // Draw background
    ClearBackground(background_color);
    
    // Draw title
    const char* title = "Turn Them";
    Vector2 title_size = MeasureTextEx(title_font, title, 72, 1);
    Vector2 title_pos = {
        (GetScreenWidth() - title_size.x) / 2.0f,
        GetScreenHeight() / 2.0f - 150.0f
    };
    DrawTextEx(title_font, title, title_pos, 72, 1, title_color);
    
    // Draw finding match text with animated dots
    std::string finding_text = "Finding Match";
    for (int i = 0; i < dot_count; i++) {
        finding_text += ".";
    }
    
    Vector2 finding_size = MeasureTextEx(button_font, finding_text.c_str(), 32, 1);
    Vector2 finding_pos = {
        (GetScreenWidth() - finding_size.x) / 2.0f,
        GetScreenHeight() / 2.0f + 50.0f
    };
    DrawTextEx(button_font, finding_text.c_str(), finding_pos, 32, 1, finding_text_color);
}

void HomeScreen::Update() {
    if (current_state == HomeScreenState::MENU) {
        Vector2 mouse_pos = GetMousePosition();
        bool mouse_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        
        // Check button hover
        is_button_hovered = CheckCollisionPointRec(mouse_pos, find_match_button);
        
        // Check button click
        if (IsButtonClicked(find_match_button, mouse_pos, mouse_pressed)) {
            current_state = HomeScreenState::FINDING_MATCH;
            animation_timer = 0.0f;
            dot_count = 0;
            curr_count = 0; // Reset counter when starting to find match
            printf("Started finding match...\n");
        }
    } else if (current_state == HomeScreenState::FINDING_MATCH) {
        UpdateFindingMatchAnimation();
        curr_count++;
        
        // Debug output every 60 frames (1 second at 60 FPS)
        if (curr_count % 60 == 0) {
            printf("Finding match... count: %d\n", curr_count);
        }
        
        if(curr_count > 180) { // Changed to 3 seconds (180 frames at 60 FPS)
            current_state = HomeScreenState::MATCH_FOUND;
            printf("Match found! Exiting home screen...\n");
        }
        // TODO: Add networking logic here to actually find a match
        // For now, this will run indefinitely until networking is implemented
    }
}

void HomeScreen::Draw() {
    if (current_state == HomeScreenState::MENU) {
        // Draw background
        ClearBackground(background_color);
        
        // Draw title
        const char* title = "Turn Them";
        Vector2 title_size = MeasureTextEx(title_font, title, 72, 1);
        Vector2 title_pos = {
            (GetScreenWidth() - title_size.x) / 2.0f,
            GetScreenHeight() / 2.0f - 150.0f
        };
        DrawTextEx(title_font, title, title_pos, 72, 1, title_color);
        
        // Draw find match button
        Color current_button_color = is_button_hovered ? button_hover_color : button_color;
        DrawRoundedButton(find_match_button, 0.3f, current_button_color, "Find Match", button_font, button_text_color);
        
    } else if (current_state == HomeScreenState::FINDING_MATCH) {
        DrawFindingMatchScreen();
    } else if (current_state == HomeScreenState::MATCH_FOUND) {
        // Draw background
        ClearBackground(background_color);
        
        // Draw title
        const char* title = "Turn Them";
        Vector2 title_size = MeasureTextEx(title_font, title, 72, 1);
        Vector2 title_pos = {
            (GetScreenWidth() - title_size.x) / 2.0f,
            GetScreenHeight() / 2.0f - 150.0f
        };
        DrawTextEx(title_font, title, title_pos, 72, 1, title_color);
        
        // Draw match found text
        const char* match_text = "Match Found!";
        Vector2 match_size = MeasureTextEx(button_font, match_text, 32, 1);
        Vector2 match_pos = {
            (GetScreenWidth() - match_size.x) / 2.0f,
            GetScreenHeight() / 2.0f + 50.0f
        };
        DrawTextEx(button_font, match_text, match_pos, 32, 1, finding_text_color);
    }
}

void HomeScreen::RunHomeScreen() {
    //homescreen loop
    while (!WindowShouldClose()){
        Update();
        
        BeginDrawing();
        Draw();
        EndDrawing();
        
        // TODO: break out of loop when match is found
        if(current_state == HomeScreenState::MATCH_FOUND){break;}
    }
}
