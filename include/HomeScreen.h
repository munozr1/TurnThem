#pragma once
#include <string>
#include "raylib.h"

enum class HomeAction {
    None,
    FindMatch,
    Practice,
    Cancel
};

enum class HomeStatus {
    Menu,       // buttons
    Connecting, // reaching the server
    Searching   // queued, waiting for an opponent
};

// Title screen. Non-blocking: the app calls Update/Draw every frame and acts on
// the returned HomeAction, so the network keeps being serviced meanwhile.
class HomeScreen {
private:
    Font title_font;
    Font button_font;
    bool font_loaded;

    HomeStatus status;
    std::string error_message;
    std::string footer;

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
    Rectangle ButtonRect(int index) const;
    void DrawRoundedButton(Rectangle rect, float roundness, Color color, const char* text, Font font, Color text_color) const;
    bool IsButtonClicked(Rectangle button, Vector2 mouse_pos, bool mouse_pressed) const;
    void DrawButton(int index, const char* text) const;

public:
    HomeScreen();
    ~HomeScreen();

    HomeAction Update();
    void Draw() const;

    void SetStatus(HomeStatus new_status);
    HomeStatus Status() const { return status; }
    void SetError(const std::string& message) { error_message = message; }
    void SetFooter(const std::string& text) { footer = text; }
};
