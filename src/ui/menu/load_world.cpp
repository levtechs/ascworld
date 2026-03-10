#include "ui/menu/load_world.h"
#include <ctime>

void LoadWorldScreen::setSaves(const std::vector<SaveSummary>& saves) {
    m_saves = saves;
    if (m_selected > (int)m_saves.size())
        m_selected = (int)m_saves.size();
}

std::string LoadWorldScreen::formatTimestamp(int64_t timestamp) {
    if (timestamp == 0) return "Unknown";
    time_t t = static_cast<time_t>(timestamp);
    struct tm* tm = localtime(&t);
    if (!tm) return "Unknown";
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
    return std::string(buf);
}

MenuResult LoadWorldScreen::update(InputState& input, int /*screenW*/, int /*screenH*/) {
    auto p = consumePressFlags(input);
    int totalItems = (int)m_saves.size() + 1; // +1 for New Game

    if (p.up)   { m_selected--; if (m_selected < 0) m_selected = totalItems - 1; }
    if (p.down) { m_selected++; if (m_selected >= totalItems) m_selected = 0; }

    if (p.confirm) return (m_selected == 0) ? MenuResult::NewGame : MenuResult::LoadWorld;
    if (p.back) return MenuResult::Back;

    return MenuResult::None;
}

void LoadWorldScreen::render(int screenW, int screenH) const {
    std::string output;
    beginOutput(output, screenW, screenH);

    int headerY = 3;
    int listStartY = 6;
    int maxVisible = screenH - 10;
    if (maxVisible < 3) maxVisible = 3;

    int totalItems = (int)m_saves.size() + 1;

    int scrollOffset = 0;
    if (totalItems > maxVisible) {
        scrollOffset = m_selected - maxVisible / 2;
        if (scrollOffset < 0) scrollOffset = 0;
        if (scrollOffset > totalItems - maxVisible) scrollOffset = totalItems - maxVisible;
    }

    for (int y = 0; y < screenH; y++) {
        bool rendered = false;

        if (y == headerY) {
            renderCenteredText(output, "=== Select World ===", screenW, TITLE_FG, BG);
            rendered = true;
        }

        int listIdx = y - listStartY;
        if (y >= listStartY && listIdx < maxVisible && (listIdx + scrollOffset) < totalItems) {
            int itemIdx = listIdx + scrollOffset;
            bool sel = (itemIdx == m_selected);
            
            std::string line;
            line = sel ? " > " : "   ";
            
            if (itemIdx == 0) {
                line += "[ CREATE NEW WORLD ]";
                while ((int)line.size() < 52) line += ' ';
            } else {
                const SaveSummary& save = m_saves[itemIdx - 1];
                std::string nameStr = save.name;
                if ((int)nameStr.size() > 24) nameStr = nameStr.substr(0, 21) + "...";
                std::string timeStr = formatTimestamp(save.timestamp);

                line += nameStr;
                while ((int)line.size() < 30) line += ' ';
                line += timeStr;
                while ((int)line.size() < 52) line += ' ';
            }

            int padLeft = (screenW - (int)line.size()) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft && x < screenW; x++) output += ' ';
            output += (sel ? SELECTED_FG : NORMAL_FG);
            output += (sel ? SELECTED_BG : BG);
            output += line;
            output += BG;
            for (int x = padLeft + (int)line.size(); x < screenW; x++) output += ' ';
            rendered = true;
        }

        // Seed info
        if (y == listStartY + maxVisible + 1 && m_selected > 0 && !m_saves.empty()) {
            const SaveSummary& save = m_saves[m_selected - 1];
            char seedInfo[64];
            snprintf(seedInfo, sizeof(seedInfo), "Seed: %u", save.seed);
            renderCenteredText(output, seedInfo, screenW, DETAIL_FG, BG);
            rendered = true;
        }

        if (y == screenH - 2) {
            renderCenteredText(output, "UP/DOWN: Navigate  |  ENTER: Select  |  ESC: Back",
                             screenW, HINT_FG, BG);
            rendered = true;
        }

        if (!rendered) renderEmptyLine(output, screenW, BG);
        endLine(output, y, screenH);
    }

    output += "\033[0m";
    flushOutput(output);
}
