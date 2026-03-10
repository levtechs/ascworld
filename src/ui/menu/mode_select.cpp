#include "ui/menu/mode_select.h"

MenuResult ModeSelectScreen::update(InputState& input, int /*screenW*/, int /*screenH*/) {
    auto p = consumePressFlags(input);

    if (p.confirm) {
        return MenuResult::OfflinePlay;
    }

    if (p.back) return MenuResult::Quit;

    return MenuResult::None;
}

void ModeSelectScreen::render(int screenW, int screenH) const {
    std::string output;
    beginOutput(output, screenW, screenH);

    const char* items[] = { "Start Game" };
    int itemCount = 1;

    int startY = screenH / 2 - 5;
    if (startY < 2) startY = 2;
    int menuStartY = startY + TITLE_LINES + 4;

    for (int y = 0; y < screenH; y++) {
        bool rendered = false;

        // Title
        if (y >= startY && y < startY + TITLE_LINES) {
            renderTitle(output, screenW, startY, y);
            rendered = true;
        }

        // Subtitle
        if (y == startY + TITLE_LINES + 1) {
            renderCenteredText(output, "~ Press ENTER to Play ~", screenW, SUBTITLE_FG, BG);
            rendered = true;
        }

        // Menu items
        if (y == menuStartY) {
            bool sel = true; // Always selected since it's the only one

            std::string text = "  > " + std::string(items[0]) + " <  ";

            int padLeft = (screenW - (int)text.size()) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft && x < screenW; x++) output += ' ';
            output += SELECTED_FG;
            output += SELECTED_BG;
            output += text;
            output += BG;
            for (int x = padLeft + (int)text.size(); x < screenW; x++) output += ' ';
            rendered = true;
        }

        // Hint
        int hintY = menuStartY + itemCount + 2;
        if (y == hintY) {
            renderCenteredText(output, "ENTER: Select  |  ESC: Quit", screenW, HINT_FG, BG);
            rendered = true;
        }

        if (!rendered) renderEmptyLine(output, screenW, BG);
        endLine(output, y, screenH);
    }

    output += "\033[0m";
    flushOutput(output);
}
