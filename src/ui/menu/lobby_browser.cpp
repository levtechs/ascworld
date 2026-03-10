#include "ui/menu/lobby_browser.h"

MenuResult LobbyBrowserScreen::update(InputState& input, int screenW, int screenH) {
    bool refresh = false;
    auto presses = input.consumePresses();
    for (auto k : presses) {
        if (k == KeyPress::Up) { if (!m_lobbies.empty()) m_selected = (m_selected - 1 + (int)m_lobbies.size()) % (int)m_lobbies.size(); }
        else if (k == KeyPress::Down) { if (!m_lobbies.empty()) m_selected = (m_selected + 1) % (int)m_lobbies.size(); }
        else if (k == KeyPress::Confirm && !m_lobbies.empty()) return MenuResult::JoinGame;
        else if (k == KeyPress::Back) return MenuResult::Back;
        else if (k == KeyPress::KeyR) refresh = true;
    }
    if (refresh) return MenuResult::Refresh;
    return MenuResult::None;
}

void LobbyBrowserScreen::render(int screenW, int screenH) const {
    std::string out;
    beginOutput(out, screenW, screenH);
    
    int startY = 4;
    for (int y = 0; y < screenH; y++) {
        if (y == startY) renderCenteredText(out, "MULTIPLAYER LOBBIES", screenW, TITLE_FG, BG);
        else if (y == startY + 1) renderCenteredText(out, "Select a game to join", screenW, SUBTITLE_FG, BG);
        else if (y == startY + 3) {
            char header[128];
            snprintf(header, sizeof(header), "%-20s %-15s %-10s", "LOBBY NAME", "HOST", "PLAYERS");
            renderCenteredText(out, header, screenW, HINT_FG, BG);
        }
        else if (y >= startY + 4 && y < startY + 14) {
            int idx = y - (startY + 4);
            if (idx < (int)m_lobbies.size()) {
                const auto& l = m_lobbies[idx];
                const char* fg = (idx == m_selected) ? SELECTED_FG : NORMAL_FG;
                const char* bg = (idx == m_selected) ? SELECTED_BG : BG;
                char buf[128];
                snprintf(buf, sizeof(buf), "%c %-19s %-15s %d/8   ", 
                        (idx == m_selected ? '>' : ' '), 
                        l.name.substr(0, 19).c_str(), 
                        l.hostName.substr(0, 15).c_str(), 
                        l.playerCount);
                renderCenteredText(out, buf, screenW, fg, bg);
            } else if (m_lobbies.empty() && idx == 0) {
                renderCenteredText(out, "No public games found...", screenW, DETAIL_FG, BG);
            } else {
                renderEmptyLine(out, screenW, BG);
            }
        } else if (y == screenH - 2) {
            renderCenteredText(out, "[ENTER] Join Game  [R] Refresh  [BACK] Back", screenW, HINT_FG, BG);
        } else {
            renderEmptyLine(out, screenW, BG);
        }
        endLine(out, y, screenH);
    }
    flushOutput(out);
}
