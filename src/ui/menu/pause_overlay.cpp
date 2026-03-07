#include "ui/menu/pause_overlay.h"
#include "ui/menu/menu_common.h"
#include <cstdio>
#include <string>
#include <cstring>

PauseResult PauseOverlay::update(InputState& input) {
    auto presses = input.consumePresses();
    for (auto k : presses) {
        switch (k) {
        case KeyPress::Up:
        case KeyPress::Down:
            m_selected = (m_selected + 1) % NUM_ITEMS;
            break;
        case KeyPress::Confirm:
            if (m_selected == 0) return PauseResult::Resume;
            if (m_selected == 1) return PauseResult::ToMenu;
            break;
        case KeyPress::Back:
            return PauseResult::Resume;  // ESC resumes
        default:
            break;
        }
    }
    return PauseResult::None;
}

void PauseOverlay::render(int screenW, int screenH) const {
    // Popup box dimensions
    const int boxW = 26;
    const int boxH = 8;

    // Center the box
    int startX = (screenW - boxW) / 2;
    int startY = (screenH - boxH) / 2;
    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;

    // Colors
    const char* borderFg = "\033[38;2;180;170;140m";  // warm gold border
    const char* borderBg = "\033[48;2;15;18;30m";     // dark bg
    const char* titleFg  = "\033[38;2;255;230;180m";  // bright title
    const char* normalFg = "\033[38;2;120;120;130m";   // unselected
    const char* selFg    = "\033[38;2;255;230;180m";   // selected fg
    const char* selBg    = "\033[48;2;30;35;50m";      // selected bg
    const char* hintFg   = "\033[38;2;60;65;80m";      // hint
    const char* reset    = "\033[0m";

    const char* items[NUM_ITEMS] = { "Continue", "Main Menu" };

    std::string out;
    out.reserve(2048);

    // Helper: move cursor to row,col (1-indexed)
    auto moveTo = [&](int row, int col) {
        char buf[24];
        int len = snprintf(buf, sizeof(buf), "\033[%d;%dH", row, col);
        out.append(buf, len);
    };

    // Draw the box
    int row = startY + 1; // 1-indexed

    // Top border
    moveTo(row, startX + 1);
    out += borderFg;
    out += borderBg;
    out += '+';
    for (int i = 0; i < boxW - 2; i++) out += '-';
    out += '+';
    out += reset;
    row++;

    // Title line: "  PAUSED  " centered
    moveTo(row, startX + 1);
    out += borderFg;
    out += borderBg;
    out += '|';
    out += titleFg;
    out += borderBg;
    const char* title = "PAUSED";
    int titleLen = (int)strlen(title);
    int pad = (boxW - 2 - titleLen) / 2;
    for (int i = 0; i < pad; i++) out += ' ';
    out += title;
    for (int i = 0; i < boxW - 2 - pad - titleLen; i++) out += ' ';
    out += borderFg;
    out += borderBg;
    out += '|';
    out += reset;
    row++;

    // Empty line
    moveTo(row, startX + 1);
    out += borderFg;
    out += borderBg;
    out += '|';
    for (int i = 0; i < boxW - 2; i++) out += ' ';
    out += '|';
    out += reset;
    row++;

    // Menu items
    for (int i = 0; i < NUM_ITEMS; i++) {
        moveTo(row, startX + 1);
        out += borderFg;
        out += borderBg;
        out += '|';

        bool selected = (i == m_selected);
        if (selected) {
            out += selFg;
            out += selBg;
        } else {
            out += normalFg;
            out += borderBg;
        }

        const char* label = items[i];
        int labelLen = (int)strlen(label);
        int lpad = (boxW - 2 - labelLen - 4) / 2 + 2; // extra for "> " prefix
        for (int j = 0; j < lpad - 2; j++) out += ' ';
        if (selected) {
            out += "> ";
        } else {
            out += "  ";
        }
        out += label;
        int remaining = boxW - 2 - lpad - labelLen;
        for (int j = 0; j < remaining; j++) out += ' ';

        out += borderFg;
        out += borderBg;
        out += '|';
        out += reset;
        row++;
    }

    // Empty line
    moveTo(row, startX + 1);
    out += borderFg;
    out += borderBg;
    out += '|';
    for (int i = 0; i < boxW - 2; i++) out += ' ';
    out += '|';
    out += reset;
    row++;

    // Hint line
    moveTo(row, startX + 1);
    out += borderFg;
    out += borderBg;
    out += '|';
    out += hintFg;
    out += borderBg;
    const char* hint = "UP/DOWN  ENTER  ESC";
    int hintLen = (int)strlen(hint);
    int hpad = (boxW - 2 - hintLen) / 2;
    for (int i = 0; i < hpad; i++) out += ' ';
    out += hint;
    for (int i = 0; i < boxW - 2 - hpad - hintLen; i++) out += ' ';
    out += borderFg;
    out += borderBg;
    out += '|';
    out += reset;
    row++;

    // Bottom border
    moveTo(row, startX + 1);
    out += borderFg;
    out += borderBg;
    out += '+';
    for (int i = 0; i < boxW - 2; i++) out += '-';
    out += '+';
    out += reset;

    fwrite(out.data(), 1, out.size(), stdout);
    fflush(stdout);
}
