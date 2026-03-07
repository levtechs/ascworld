#include "ui/menu/connecting.h"

MenuResult ConnectingScreen::update(InputState& input, int /*screenW*/, int /*screenH*/) {
    auto p = consumePressFlags(input);
    if (p.back) return MenuResult::Disconnect;
    return MenuResult::None;
}

void ConnectingScreen::render(int screenW, int screenH) const {
    m_animFrame++;
    std::string output;
    beginOutput(output, screenW, screenH);

    int centerY = screenH / 2;

    const char spinChars[] = { '|', '/', '-', '\\' };
    int spinIdx = (m_animFrame / 10) % 4;

    std::string connectLine = "Connecting...  ";
    connectLine += spinChars[spinIdx];

    for (int y = 0; y < screenH; y++) {
        bool rendered = false;

        if (y == centerY - 2) {
            renderCenteredText(output, connectLine.c_str(), screenW, TITLE_FG, BG);
            rendered = true;
        }

        if (y == centerY) {
            renderCenteredText(output, m_status.c_str(), screenW, NORMAL_FG, BG);
            rendered = true;
        }

        if (y == centerY + 3) {
            renderCenteredText(output, "ESC: Cancel", screenW, HINT_FG, BG);
            rendered = true;
        }

        if (!rendered) renderEmptyLine(output, screenW, BG);
        endLine(output, y, screenH);
    }

    output += "\033[0m";
    flushOutput(output);
}
