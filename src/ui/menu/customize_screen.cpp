#include "ui/menu/customize_screen.h"
#include <cstring>

MenuResult CustomizeScreen::update(InputState& input, int /*screenW*/, int /*screenH*/) {
    auto p = consumePressFlags(input);

    if (p.back) return MenuResult::Back;

    if (p.up) {
        m_field--;
        if (m_field < 0) m_field = 2;
    }
    if (p.down) {
        m_field++;
        if (m_field > 2) m_field = 0;
    }

    if (m_field == 0) {
        // Color selection
        if (p.left) {
            int c = m_appearance.colorIndex;
            c--;
            if (c < 0) c = NUM_CHARACTER_COLORS - 1;
            m_appearance.colorIndex = static_cast<uint8_t>(c);
        }
        if (p.right) {
            int c = m_appearance.colorIndex;
            c++;
            if (c >= NUM_CHARACTER_COLORS) c = 0;
            m_appearance.colorIndex = static_cast<uint8_t>(c);
        }
    } else if (m_field == 1) {
        // Design selection
        if (p.left) {
            int d = static_cast<int>(m_appearance.design);
            d--;
            if (d < 0) d = static_cast<int>(CharacterDesign::COUNT) - 1;
            m_appearance.design = static_cast<CharacterDesign>(d);
        }
        if (p.right) {
            int d = static_cast<int>(m_appearance.design);
            d++;
            if (d >= static_cast<int>(CharacterDesign::COUNT)) d = 0;
            m_appearance.design = static_cast<CharacterDesign>(d);
        }
    }

    if (p.confirm && m_field == 2) {
        return MenuResult::Back;  // Done = go back to lobby
    }

    return MenuResult::None;
}

void CustomizeScreen::render(int screenW, int screenH) const {
    std::string output;
    beginOutput(output, screenW, screenH);

    int centerY = screenH / 2;
    int startY = centerY - 8;
    if (startY < 1) startY = 1;

    Color3 col = m_appearance.color();
    int cr = static_cast<int>(col.r * 255);
    int cg = static_cast<int>(col.g * 255);
    int cb = static_cast<int>(col.b * 255);
    char colorFg[32];
    snprintf(colorFg, sizeof(colorFg), "\033[38;2;%d;%d;%dm", cr, cg, cb);

    for (int y = 0; y < screenH; y++) {
        bool rendered = false;

        if (y == startY) {
            renderCenteredText(output, "=== Character Customization ===", screenW, TITLE_FG, BG);
            rendered = true;
        }

        // Color row
        if (y == startY + 3) {
            bool sel = (m_field == 0);
            char buf[128];
            snprintf(buf, sizeof(buf), "< Color: %s >",
                     CHARACTER_COLOR_NAMES[m_appearance.colorIndex]);
            std::string line = sel ? "> " : "  ";
            line += buf;

            int padLeft = (screenW - static_cast<int>(line.size())) / 2;
            if (padLeft < 0) padLeft = 0;

            output += (sel ? SELECTED_BG : BG);
            for (int x = 0; x < padLeft; x++) output += ' ';
            output += (sel ? SELECTED_FG : NORMAL_FG);
            output += line;
            output += BG;
            for (int x = padLeft + static_cast<int>(line.size()); x < screenW; x++) output += ' ';
            rendered = true;
        }

        // Design row
        if (y == startY + 5) {
            bool sel = (m_field == 1);
            char buf[128];
            snprintf(buf, sizeof(buf), "< Design: %s >",
                     CHARACTER_DESIGN_NAMES[static_cast<int>(m_appearance.design)]);
            std::string line = sel ? "> " : "  ";
            line += buf;

            int padLeft = (screenW - static_cast<int>(line.size())) / 2;
            if (padLeft < 0) padLeft = 0;

            output += (sel ? SELECTED_BG : BG);
            for (int x = 0; x < padLeft; x++) output += ' ';
            output += (sel ? SELECTED_FG : NORMAL_FG);
            output += line;
            output += BG;
            for (int x = padLeft + static_cast<int>(line.size()); x < screenW; x++) output += ' ';
            rendered = true;
        }

        // ASCII art preview of character
        if (y == startY + 8) {
            // Head line
            const char* headArt = nullptr;
            switch (m_appearance.design) {
                case CharacterDesign::Standard: headArt = "  (O)  "; break;
                case CharacterDesign::Blocky:   headArt = "  [O]  "; break;
                case CharacterDesign::Slim:     headArt = "  .o.  "; break;
                default:                        headArt = "  (O)  "; break;
            }
            int padLeft = (screenW - 7) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft; x++) output += ' ';
            output += colorFg;
            output += headArt;
            output += BG;
            for (int x = padLeft + 7; x < screenW; x++) output += ' ';
            rendered = true;
        }

        if (y == startY + 9) {
            const char* bodyArt1 = nullptr;
            switch (m_appearance.design) {
                case CharacterDesign::Standard: bodyArt1 = "  /|\\  "; break;
                case CharacterDesign::Blocky:   bodyArt1 = " [|||] "; break;
                case CharacterDesign::Slim:     bodyArt1 = "  /|\\  "; break;
                default:                        bodyArt1 = "  /|\\  "; break;
            }
            int padLeft = (screenW - 7) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft; x++) output += ' ';
            output += colorFg;
            output += bodyArt1;
            output += BG;
            for (int x = padLeft + 7; x < screenW; x++) output += ' ';
            rendered = true;
        }

        if (y == startY + 10) {
            const char* bodyArt2 = nullptr;
            switch (m_appearance.design) {
                case CharacterDesign::Standard: bodyArt2 = "  |#|  "; break;
                case CharacterDesign::Blocky:   bodyArt2 = " [###] "; break;
                case CharacterDesign::Slim:     bodyArt2 = "   |   "; break;
                default:                        bodyArt2 = "  |#|  "; break;
            }
            int padLeft = (screenW - 7) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft; x++) output += ' ';
            output += colorFg;
            output += bodyArt2;
            output += BG;
            for (int x = padLeft + 7; x < screenW; x++) output += ' ';
            rendered = true;
        }

        if (y == startY + 11) {
            const char* bodyArt3 = nullptr;
            switch (m_appearance.design) {
                case CharacterDesign::Standard: bodyArt3 = "  / \\  "; break;
                case CharacterDesign::Blocky:   bodyArt3 = " [/ \\] "; break;
                case CharacterDesign::Slim:     bodyArt3 = "  / \\  "; break;
                default:                        bodyArt3 = "  / \\  "; break;
            }
            int padLeft = (screenW - 7) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft; x++) output += ' ';
            output += colorFg;
            output += bodyArt3;
            output += BG;
            for (int x = padLeft + 7; x < screenW; x++) output += ' ';
            rendered = true;
        }

        // Done button
        if (y == startY + 14) {
            bool sel = (m_field == 2);
            renderCenteredText(output, sel ? "> [Done] <" : "  [Done]  ", screenW,
                               sel ? SELECTED_FG : NORMAL_FG,
                               sel ? SELECTED_BG : BG);
            rendered = true;
        }

        // Hints
        if (y == screenH - 2) {
            renderCenteredText(output, "UP/DOWN: Field | LEFT/RIGHT: Change | ENTER: Confirm | ESC: Back",
                             screenW, HINT_FG, BG);
            rendered = true;
        }

        if (!rendered) renderEmptyLine(output, screenW, BG);
        endLine(output, y, screenH);
    }

    output += "\033[0m";
    flushOutput(output);
}
