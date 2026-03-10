#include "ui/menu/main_menu.h"

std::vector<std::string> MainMenuScreen::getItems() const {
    std::vector<std::string> items;
    if (m_canContinue) items.push_back("Resume");
    items.push_back("Play");
    items.push_back("Customize");
    items.push_back("Quit");
    return items;
}

MenuResult MainMenuScreen::update(InputState& input, int /*screenW*/, int /*screenH*/) {
    auto p = consumePressFlags(input);
    auto items = getItems();
    int count = (int)items.size();

    if (p.up)   { m_selected--; if (m_selected < 0) m_selected = count - 1; }
    if (p.down) { m_selected++; if (m_selected >= count) m_selected = 0; }
    if (m_selected >= count) m_selected = 0;

    if (p.confirm && m_selected < count) {
        const std::string& chosen = items[m_selected];
        if (chosen == "Resume") return MenuResult::Continue;
        if (chosen == "Play") return MenuResult::Play;
        if (chosen == "Customize") return MenuResult::CustomizeChar;
        if (chosen == "Quit") return MenuResult::Quit;
    }

    if (p.back) {
        // In main menu, back does nothing or same as quit
    }

    return MenuResult::None;
}

void MainMenuScreen::render(int screenW, int screenH) const {
    std::string output;
    beginOutput(output, screenW, screenH);

    auto items = getItems();
    int itemCount = (int)items.size();

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
            renderCenteredText(output, "~ A Terminal 3D World ~", screenW, SUBTITLE_FG, BG);
            rendered = true;
        }

        // Menu items
        if (y >= menuStartY && y < menuStartY + itemCount) {
            int idx = y - menuStartY;
            bool sel = (idx == m_selected);

            std::string text;
            if (sel) text = "  > " + items[idx] + " <  ";
            else     text = "    " + items[idx] + "    ";

            int padLeft = (screenW - (int)text.size()) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft && x < screenW; x++) output += ' ';
            output += (sel ? SELECTED_FG : NORMAL_FG);
            output += (sel ? SELECTED_BG : BG);
            output += text;
            output += BG;
            for (int x = padLeft + (int)text.size(); x < screenW; x++) output += ' ';
            rendered = true;
        }

        // Hint
        if (y == menuStartY + itemCount + 2) {
            renderCenteredText(output, "UP/DOWN: Navigate  |  ENTER: Select", screenW, HINT_FG, BG);
            rendered = true;
        }

        if (!rendered) renderEmptyLine(output, screenW, BG);
        endLine(output, y, screenH);
    }

    output += "\033[0m";
    flushOutput(output);
}
