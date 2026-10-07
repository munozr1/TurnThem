#include "HomeScreen.h"
#include <string>

HomeScreen::HomeScreen() {
    status = HomeStatus::Menu;
    font_loaded = false;
    animation_timer = 0.0f;
    dot_count = 0;

    // Initialize colors (Clash Royale inspired)
    background_color = {41, 128, 185, 255}; // Blue background
    title_color = {255, 255, 255, 255}; // White title
    button_color = {46, 204, 113, 255}; // Green button
    button_hover_color = {39, 174, 96, 255}; // Darker green on hover
    button_text_color = {255, 255, 255, 255}; // White button text
    finding_text_color = {255, 255, 255, 255}; // White finding text

    InitializeFonts();
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

void HomeScreen::SetStatus(HomeStatus new_status) {
    status = new_status;
    animation_timer = 0.0f;
    dot_count = 0;
    if (status != HomeStatus::Menu) error_message.clear();
}

// buttons stack below the middle of the screen: 0 = top, 1 = below it
Rectangle HomeScreen::ButtonRect(int index) const {
    const float width = 220.0f, height = 60.0f;
    return {(GetScreenWidth() - width) / 2.0f, GetScreenHeight() / 2.0f + 40.0f + index * 80.0f, width, height};
}

void HomeScreen::DrawRoundedButton(Rectangle rect, float roundness, Color color, const char* text, Font font, Color text_color) const {
    // Add subtle shadow effect
    Rectangle shadow_rect = {rect.x + 2, rect.y + 2, rect.width, rect.height};
    DrawRectangleRounded(shadow_rect, roundness, 16, {0, 0, 0, 50});
    DrawRectangleRounded(rect, roundness, 16, color);

    // Center text in button
    const float size = 24.0f;
    Vector2 text_size = MeasureTextEx(font, text, size, 2);
    Vector2 text_pos = {
        rect.x + (rect.width - text_size.x) / 2.0f,
        rect.y + (rect.height - text_size.y) / 2.0f
    };

    DrawTextEx(font, text, text_pos, size, 2, text_color);
}

bool HomeScreen::IsButtonClicked(Rectangle button, Vector2 mouse_pos, bool mouse_pressed) const {
    return CheckCollisionPointRec(mouse_pos, button) && mouse_pressed;
}

void HomeScreen::DrawButton(int index, const char* text) const {
    const Rectangle rect = ButtonRect(index);
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), rect);
    DrawRoundedButton(rect, 0.3f, hovered ? button_hover_color : button_color, text, button_font, button_text_color);
}

HomeAction HomeScreen::Update() {
    const Vector2 mouse_pos = GetMousePosition();
    const bool mouse_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (status == HomeStatus::Menu) {
        if (IsButtonClicked(ButtonRect(0), mouse_pos, mouse_pressed)) return HomeAction::FindMatch;
        if (IsButtonClicked(ButtonRect(1), mouse_pos, mouse_pressed)) return HomeAction::Practice;
        return HomeAction::None;
    }

    // Update dots every 0.5 seconds
    animation_timer += GetFrameTime();
    if (animation_timer >= 0.5f) {
        dot_count = (dot_count + 1) % 4; // 0, 1, 2, 3 dots
        animation_timer = 0.0f;
    }
    if (IsButtonClicked(ButtonRect(1), mouse_pos, mouse_pressed)) return HomeAction::Cancel;
    return HomeAction::None;
}

void HomeScreen::Draw() const {
    ClearBackground(background_color);

    // Draw title
    const char* title = "Turn Them";
    Vector2 title_size = MeasureTextEx(title_font, title, 72, 1);
    Vector2 title_pos = {
        (GetScreenWidth() - title_size.x) / 2.0f,
        GetScreenHeight() / 2.0f - 150.0f
    };
    DrawTextEx(title_font, title, title_pos, 72, 1, title_color);

    if (status == HomeStatus::Menu) {
        DrawButton(0, "Find Match");
        DrawButton(1, "Practice");
        if (!error_message.empty()) {
            const Vector2 size = MeasureTextEx(button_font, error_message.c_str(), 20, 2);
            DrawTextEx(button_font, error_message.c_str(),
                       {(GetScreenWidth() - size.x) / 2.0f, ButtonRect(1).y + 90.0f}, 20, 2, Color{255, 220, 220, 255});
        }
    } else {
        // Draw finding match text with animated dots
        std::string finding_text = status == HomeStatus::Connecting ? "Connecting" : "Finding Match";
        for (int i = 0; i < dot_count; i++) {
            finding_text += ".";
        }
        // measure without dots so the text doesn't jump around
        const float width = MeasureTextEx(button_font, status == HomeStatus::Connecting ? "Connecting" : "Finding Match", 32, 2).x;
        DrawTextEx(button_font, finding_text.c_str(), {(GetScreenWidth() - width) / 2.0f, ButtonRect(0).y + 14.0f}, 32, 2, finding_text_color);
        DrawButton(1, "Cancel");
    }

    if (!footer.empty()) {
        const Vector2 size = MeasureTextEx(button_font, footer.c_str(), 16, 2);
        DrawTextEx(button_font, footer.c_str(), {(GetScreenWidth() - size.x) / 2.0f, GetScreenHeight() - 40.0f}, 16, 2,
                   Color{220, 230, 240, 255});
    }
}
