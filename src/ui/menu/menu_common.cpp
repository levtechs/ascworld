#include "ui/menu/menu_common.h"

// ---- ASCII art title lines ----
static const char* TITLE_ART[] = {
    R"(    ___   _____ _______       ______  ____  __    ____ )",
    R"(   /   | / ___// ____/ |     / / __ \/ __ \/ /   / __ \)",
    R"(  / /| | \__ \/ /    | | /| / / / / / /_/ / /   / / / /)",
    R"( / ___ |___/ / /___  | |/ |/ / /_/ / _, _/ /___/ /_/ / )",
    R"(/_/  |_/____/\____/  |__/|__/\____/_/ |_/_____/_____/  )",
};

void MenuScreen::renderTitle(std::string& output, int screenW, int startY, int y) {
    int tIdx = y - startY;
    if (tIdx < 0 || tIdx >= TITLE_LINES) return;

    int padLeft = (screenW - TITLE_WIDTH) / 2;
    if (padLeft < 0) padLeft = 0;

    output += TITLE_FG;
    output += BG;
    for (int x = 0; x < padLeft && x < screenW; x++) output += ' ';
    const char* line = TITLE_ART[tIdx];
    int len = (int)strlen(line);
    for (int x = 0; x < len && (padLeft + x) < screenW; x++) output += line[x];
    for (int x = padLeft + len; x < screenW; x++) output += ' ';
}

void MenuScreen::renderCenteredText(std::string& output, const char* text, int screenW,
                                     const char* fgColor, const char* bgColor) {
    int textLen = (int)strlen(text);
    int padLeft = (screenW - textLen) / 2;
    if (padLeft < 0) padLeft = 0;

    output += fgColor;
    output += bgColor;
    for (int x = 0; x < padLeft && x < screenW; x++) output += ' ';
    for (int x = 0; x < textLen && (padLeft + x) < screenW; x++) output += text[x];
    for (int x = padLeft + textLen; x < screenW; x++) output += ' ';
}

void MenuScreen::renderEmptyLine(std::string& output, int screenW, const char* bgColor) {
    output += bgColor;
    for (int i = 0; i < screenW; i++) output += ' ';
}

MenuScreen::PressFlags MenuScreen::consumePressFlags(InputState& input) {
    PressFlags f;
    auto presses = input.consumePresses();
    for (auto k : presses) {
        switch (k) {
            case KeyPress::Up:      f.up = true; break;
            case KeyPress::Down:    f.down = true; break;
            case KeyPress::Left:    f.left = true; break;
            case KeyPress::Right:   f.right = true; break;
            case KeyPress::Confirm: f.confirm = true; break;
            case KeyPress::Back:    f.back = true; break;
            default: break;  // Ignore game keys (Q/E/F/G/1-9/MouseLeft) in menus
        }
    }
    return f;
}
