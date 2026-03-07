#include "ui/menu/load_world.h"
#include <ctime>

void LoadWorldScreen::setSaves(const std::vector<SaveData>& saves) {
    m_saves = saves;
    if (m_selected >= (int)m_saves.size())
        m_selected = m_saves.empty() ? 0 : (int)m_saves.size() - 1;
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
    int count = (int)m_saves.size();

    if (count == 0) return MenuResult::Back;

    if (p.up)   { m_selected--; if (m_selected < 0) m_selected = count - 1; }
    if (p.down) { m_selected++; if (m_selected >= count) m_selected = 0; }

    if (p.confirm) return MenuResult::LoadWorld;
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

    int total = (int)m_saves.size();

    int scrollOffset = 0;
    if (total > maxVisible) {
        scrollOffset = m_selected - maxVisible / 2;
        if (scrollOffset < 0) scrollOffset = 0;
        if (scrollOffset > total - maxVisible) scrollOffset = total - maxVisible;
    }

    for (int y = 0; y < screenH; y++) {
        bool rendered = false;

        if (y == headerY) {
            renderCenteredText(output, "=== Load World ===", screenW, TITLE_FG, BG);
            rendered = true;
        }

        int listIdx = y - listStartY;
        if (y >= listStartY && listIdx < maxVisible && (listIdx + scrollOffset) < total) {
            int saveIdx = listIdx + scrollOffset;
            const SaveData& save = m_saves[saveIdx];
            bool sel = (saveIdx == m_selected);

            std::string nameStr = save.name;
            if ((int)nameStr.size() > 24) nameStr = nameStr.substr(0, 21) + "...";
            std::string timeStr = formatTimestamp(save.timestamp);

            std::string line;
            line = sel ? " > " : "   ";
            line += nameStr;
            while ((int)line.size() < 30) line += ' ';
            line += timeStr;
            while ((int)line.size() < 52) line += ' ';

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
        if (y == listStartY + maxVisible + 1 && !m_saves.empty()) {
            const SaveData& save = m_saves[m_selected];
            char seedInfo[64];
            snprintf(seedInfo, sizeof(seedInfo), "Seed: %u", save.seed);
            renderCenteredText(output, seedInfo, screenW, DETAIL_FG, BG);
            rendered = true;
        }

        if (y == screenH - 2) {
            renderCenteredText(output, "UP/DOWN: Navigate  |  ENTER: Load  |  ESC: Back",
                             screenW, HINT_FG, BG);
            rendered = true;
        }

        if (!rendered) renderEmptyLine(output, screenW, BG);
        endLine(output, y, screenH);
    }

    output += "\033[0m";
    flushOutput(output);
}
