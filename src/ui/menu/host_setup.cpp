#include "ui/menu/host_setup.h"

MenuResult HostSetupScreen::update(InputState& input, int /*screenW*/, int /*screenH*/) {
    // Host setup uses text input mode for room name and password fields
    bool nameOrPassField = (m_field == 0 || (m_field == 2 && !m_isPublic));
    input.textInputMode.store(nameOrPassField, std::memory_order_relaxed);

    auto p = consumePressFlags(input);

    if (p.back) {
        input.textInputMode.store(false, std::memory_order_relaxed);
        return MenuResult::Back;
    }

    int startField = m_isPublic ? 2 : 3;

    if (nameOrPassField) {
        // Text input for the active field
        std::string chars;
        int backspaces = 0;
        bool enterHit = false;
        input.consumeTextInput(chars, backspaces, enterHit);

        std::string& target = (m_field == 0) ? m_roomName : m_password;

        for (int i = 0; i < backspaces; i++) {
            if (!target.empty()) target.pop_back();
        }
        for (char c : chars) {
            if ((int)target.size() >= 16) break;
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ' ') {
                target += c;
            }
        }

        // Enter moves to next field
        if (enterHit) {
            m_field++;
            if (m_isPublic && m_field == 2) m_field = startField; // skip password
        }
    } else {
        // Navigation with press events
        if (p.up) {
            m_field--;
            if (m_field < 0) m_field = startField;
            if (m_isPublic && m_field == 2) m_field = 1;
        }
        if (p.down) {
            m_field++;
            if (m_field > startField) m_field = 0;
            if (m_isPublic && m_field == 2) m_field = startField;
        }

        if (m_field == 1 && p.confirm) {
            m_isPublic = !m_isPublic;
            if (m_isPublic) m_password.clear();
        }

        if (m_field == startField && p.confirm) {
            input.textInputMode.store(false, std::memory_order_relaxed);
            return MenuResult::HostGame;
        }
    }

    return MenuResult::None;
}

void HostSetupScreen::render(int screenW, int screenH) const {
    m_animFrame++;
    std::string output;
    beginOutput(output, screenW, screenH);

    int centerY = screenH / 2;
    int headerY = centerY - 7;
    int fieldStartY = centerY - 3;
    int startField = m_isPublic ? 2 : 3;

    for (int y = 0; y < screenH; y++) {
        bool rendered = false;

        if (y == headerY) {
            renderCenteredText(output, "=== Host Game ===", screenW, TITLE_FG, BG);
            rendered = true;
        }

        // Room Name
        if (y == fieldStartY) {
            bool sel = (m_field == 0);
            std::string label = "Room Name: ";
            std::string fieldText;
            if (sel) {
                fieldText = m_roomName;
                bool blink = (m_animFrame / 15) % 2 == 0;
                fieldText += blink ? "_" : " ";
            } else {
                fieldText = m_roomName.empty() ? "(empty)" : m_roomName;
            }

            std::string fullLine = (sel ? " > " : "   ") + label + fieldText;
            int padLeft = (screenW - (int)fullLine.size()) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft && x < screenW; x++) output += ' ';
            output += (sel ? SELECTED_FG : NORMAL_FG);
            output += (sel ? SELECTED_BG : BG);
            output += fullLine;
            output += BG;
            for (int x = padLeft + (int)fullLine.size(); x < screenW; x++) output += ' ';
            rendered = true;
        }

        // Visibility
        if (y == fieldStartY + 2) {
            bool sel = (m_field == 1);
            std::string visText = m_isPublic ? "[Public]  Private " : " Public  [Private]";
            std::string fullLine = (sel ? " > " : "   ") + std::string("Visibility: ") + visText;

            int padLeft = (screenW - (int)fullLine.size()) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft && x < screenW; x++) output += ' ';
            output += (sel ? SELECTED_FG : NORMAL_FG);
            output += (sel ? SELECTED_BG : BG);
            output += fullLine;
            output += BG;
            for (int x = padLeft + (int)fullLine.size(); x < screenW; x++) output += ' ';
            rendered = true;
        }

        // Password (only if private)
        if (!m_isPublic && y == fieldStartY + 4) {
            bool sel = (m_field == 2);
            std::string label = "Password: ";
            std::string fieldText;
            if (sel) {
                fieldText = m_password;
                bool blink = (m_animFrame / 15) % 2 == 0;
                fieldText += blink ? "_" : " ";
            } else {
                fieldText = m_password.empty() ? "(empty)" : m_password;
            }

            std::string fullLine = (sel ? " > " : "   ") + label + fieldText;
            int padLeft = (screenW - (int)fullLine.size()) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft && x < screenW; x++) output += ' ';
            output += (sel ? SELECTED_FG : NORMAL_FG);
            output += (sel ? SELECTED_BG : BG);
            output += fullLine;
            output += BG;
            for (int x = padLeft + (int)fullLine.size(); x < screenW; x++) output += ' ';
            rendered = true;
        }

        // Start button
        int startButtonY = m_isPublic ? fieldStartY + 4 : fieldStartY + 6;
        if (y == startButtonY) {
            bool sel = (m_field == startField);
            std::string btnText = sel ? "  > [Start Hosting] <  " : "    [Start Hosting]    ";

            int padLeft = (screenW - (int)btnText.size()) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft && x < screenW; x++) output += ' ';
            output += (sel ? SELECTED_FG : NORMAL_FG);
            output += (sel ? SELECTED_BG : BG);
            output += btnText;
            output += BG;
            for (int x = padLeft + (int)btnText.size(); x < screenW; x++) output += ' ';
            rendered = true;
        }

        if (y == screenH - 3) {
            renderCenteredText(output, "UP/DOWN: Navigate | ENTER: Toggle/Confirm | Type to edit",
                             screenW, HINT_FG, BG);
            rendered = true;
        }
        if (y == screenH - 2) {
            renderCenteredText(output, "ESC: Back to Lobby", screenW, HINT_FG, BG);
            rendered = true;
        }

        if (!rendered) renderEmptyLine(output, screenW, BG);
        endLine(output, y, screenH);
    }

    output += "\033[0m";
    flushOutput(output);
}
