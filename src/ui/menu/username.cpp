#include "ui/menu/username.h"

MenuResult UsernameScreen::update(InputState& input, int /*screenW*/, int /*screenH*/) {
    // Enable text input mode so CGEventTap captures typed chars
    input.textInputMode.store(true, std::memory_order_relaxed);

    // Consume typed characters
    std::string chars;
    int backspaces = 0;
    bool enterHit = false;
    input.consumeTextInput(chars, backspaces, enterHit);

    // Apply backspaces
    for (int i = 0; i < backspaces; i++) {
        if (!m_username.empty()) m_username.pop_back();
    }

    // Apply typed characters (filter to allowed set)
    for (char c : chars) {
        if ((int)m_username.size() >= MAX_LEN) break;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
            m_username += c;
        }
    }

    // Enter: confirm
    if (enterHit && !m_username.empty()) {
        input.textInputMode.store(false, std::memory_order_relaxed);
        return MenuResult::OnlinePlay;
    }

    // Also check press queue for Back (Esc)
    auto p = consumePressFlags(input);
    if (p.back) {
        input.textInputMode.store(false, std::memory_order_relaxed);
        return MenuResult::Back;
    }

    return MenuResult::None;
}

void UsernameScreen::render(int screenW, int screenH) const {
    m_animFrame++;
    std::string output;
    beginOutput(output, screenW, screenH);

    int centerY = screenH / 2;
    int headerY = centerY - 3;
    int inputY = centerY;
    int hintY = centerY + 3;

    bool showCursor = (m_animFrame / 15) % 2 == 0;
    std::string inputText = "> " + m_username;
    if (showCursor) inputText += "_";

    int fieldWidth = MAX_LEN + 6;

    for (int y = 0; y < screenH; y++) {
        bool rendered = false;

        if (y == headerY) {
            renderCenteredText(output, "Enter Username", screenW, TITLE_FG, BG);
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
            renderCenteredText(output, "Type your name, then press ENTER", screenW, HINT_FG, BG);
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
