#include "ui/menu/join_password.h"

MenuResult JoinPasswordScreen::update(InputState& input, int /*screenW*/, int /*screenH*/) {
    // Use text input mode for password entry
    input.textInputMode.store(true, std::memory_order_relaxed);

    std::string chars;
    int backspaces = 0;
    bool enterHit = false;
    input.consumeTextInput(chars, backspaces, enterHit);

    for (int i = 0; i < backspaces; i++) {
        if (!m_password.empty()) m_password.pop_back();
    }
    for (char c : chars) {
        if ((int)m_password.size() >= 16) break;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
            m_password += c;
        }
    }

    if (enterHit) {
        input.textInputMode.store(false, std::memory_order_relaxed);
        return MenuResult::JoinGame;
    }

    auto p = consumePressFlags(input);
    if (p.back) {
        input.textInputMode.store(false, std::memory_order_relaxed);
        return MenuResult::Back;
    }

    return MenuResult::None;
}

void JoinPasswordScreen::render(int screenW, int screenH) const {
    m_animFrame++;
    std::string output;
    beginOutput(output, screenW, screenH);

    int centerY = screenH / 2;
    int headerY = centerY - 4;
    int promptY = centerY - 1;
    int inputY = centerY + 1;
    int hintY = centerY + 5;

    bool showCursor = (m_animFrame / 15) % 2 == 0;
    std::string inputText = "> " + m_password;
    if (showCursor) inputText += "_";
    int fieldWidth = 22;

    for (int y = 0; y < screenH; y++) {
        bool rendered = false;

        if (y == headerY) {
            renderCenteredText(output, "=== Join Room ===", screenW, TITLE_FG, BG);
            rendered = true;
        }

        if (y == promptY) {
            renderCenteredText(output, "Enter Password:", screenW, NORMAL_FG, BG);
            rendered = true;
        }

        if (y == inputY) {
            int padLeft = (screenW - fieldWidth) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft; x++) output += ' ';

            output += INPUT_FG;
            output += INPUT_BG;
            int col = 0;
            for (int i = 0; i < (int)inputText.size() && col < fieldWidth; i++, col++)
                output += inputText[i];
            for (; col < fieldWidth; col++) output += ' ';

            output += BG;
            for (int x = padLeft + fieldWidth; x < screenW; x++) output += ' ';
            rendered = true;
        }

        if (y == hintY) {
            renderCenteredText(output, "Type password, then press ENTER", screenW, HINT_FG, BG);
            rendered = true;
        }
        if (y == hintY + 1) {
            renderCenteredText(output, "ESC: Back", screenW, HINT_FG, BG);
            rendered = true;
        }

        if (!rendered) renderEmptyLine(output, screenW, BG);
        endLine(output, y, screenH);
    }

    output += "\033[0m";
    flushOutput(output);
}
