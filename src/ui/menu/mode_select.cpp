#include "ui/menu/mode_select.h"

MenuResult ModeSelectScreen::update(InputState& input, int /*screenW*/, int /*screenH*/) {
    auto p = consumePressFlags(input);

    if (p.up)   { m_selected--; if (m_selected < 0) m_selected = 1; }
    if (p.down) { m_selected++; if (m_selected > 1) m_selected = 0; }

    if (p.confirm) {
        if (m_selected == 0) return MenuResult::OnlinePlay;
        if (m_selected == 1) return MenuResult::OfflinePlay;
    }

    if (p.back) return MenuResult::Quit;

    return MenuResult::None;
}

void ModeSelectScreen::render(int screenW, int screenH) const {
    std::string output;
    beginOutput(output, screenW, screenH);

    const char* items[] = { "Online Play", "Offline Play" };
    int itemCount = 2;

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
            renderCenteredText(output, "~ Select Play Mode ~", screenW, SUBTITLE_FG, BG);
            rendered = true;
        }

        // Menu items
        if (y >= menuStartY && y < menuStartY + itemCount) {
            int idx = y - menuStartY;
            bool sel = (idx == m_selected);

            std::string text;
            if (sel) text = "  > " + std::string(items[idx]) + " <  ";
            else     text = "    " + std::string(items[idx]) + "    ";

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

        // Error message (if any)
        if (!m_error.empty() && y == menuStartY + itemCount + 2) {
            renderCenteredText(output, m_error.c_str(), screenW, "\033[38;2;220;80;80m", BG);
            rendered = true;
        }

        // Hint
        int hintY = menuStartY + itemCount + (m_error.empty() ? 2 : 4);
        if (y == hintY) {
            renderCenteredText(output, "UP/DOWN: Navigate  |  ENTER: Select  |  ESC: Quit", screenW, HINT_FG, BG);
            rendered = true;
        }

        if (!rendered) renderEmptyLine(output, screenW, BG);
        endLine(output, y, screenH);
    }

    output += "\033[0m";
    flushOutput(output);
}
