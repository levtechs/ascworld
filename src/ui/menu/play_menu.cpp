#include "ui/menu/play_menu.h"

MenuResult PlayMenuScreen::update(InputState& input, int screenW, int screenH) {
    auto press = consumePressFlags(input);
    if (press.up) m_selected = (m_selected - 1 + (int)m_items.size()) % (int)m_items.size();
    if (press.down) m_selected = (m_selected + 1) % (int)m_items.size();
    if (press.confirm) return m_items[m_selected].second;
    if (press.back) return MenuResult::Back;
    return MenuResult::None;
}

void PlayMenuScreen::render(int screenW, int screenH) const {
    std::string out;
    beginOutput(out, screenW, screenH);
    
    int startY = screenH / 2 - 5;
    for (int y = 0; y < screenH; y++) {
        if (y >= startY && y < startY + TITLE_LINES) renderTitle(out, screenW, startY, y);
        else if (y == startY + 6) renderCenteredText(out, "SELECT GAME MODE", screenW, SUBTITLE_FG, BG);
        else if (y >= startY + 8 && y < startY + 8 + (int)m_items.size()) {
            int idx = y - (startY + 8);
            const char* fg = (idx == m_selected) ? SELECTED_FG : NORMAL_FG;
            const char* bg = (idx == m_selected) ? SELECTED_BG : BG;
            char buf[64];
            snprintf(buf, sizeof(buf), (idx == m_selected) ? "> %s <" : "  %s  ", m_items[idx].first.c_str());
            renderCenteredText(out, buf, screenW, fg, bg);
        } else {
            renderEmptyLine(out, screenW, BG);
        }
        endLine(out, y, screenH);
    }
    flushOutput(out);
}
