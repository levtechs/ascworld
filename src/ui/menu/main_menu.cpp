#include "ui/menu/main_menu.h"

std::vector<std::string> MainMenuScreen::getItems() const {
    std::vector<std::string> items;
    if (m_canContinue) items.push_back("Continue");
    items.push_back("New Game");
    if (!m_saves.empty()) items.push_back("Load World");
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
        if (chosen == "Continue") return MenuResult::Continue;
        if (chosen == "New Game") return MenuResult::NewGame;
        if (chosen == "Load World") return MenuResult::LoadWorld;
        if (chosen == "Quit") return MenuResult::Quit;
    }

    if (p.back) {
        return MenuResult::Back; // always go back to mode select
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
            renderCenteredText(output, "~ A Terminal 3D Explorer ~", screenW, SUBTITLE_FG, BG);
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
            renderCenteredText(output, "UP/DOWN: Navigate  |  ENTER: Select  |  ESC: Back", screenW, HINT_FG, BG);
            rendered = true;
        }

        if (!rendered) renderEmptyLine(output, screenW, BG);
        endLine(output, y, screenH);
    }

    output += "\033[0m";
    flushOutput(output);
}
