#include "ui/menu/pause_overlay.h"
#include "ui/menu/menu_common.h"
#include <cstdio>
#include <string>
#include <cstring>
#include <vector>

PauseResult PauseOverlay::update(InputState& input, bool isOnline, bool canRename, int& targetFPS) {
    // Rename mode: consume text input
    if (m_renaming) {
        std::string chars;
        int backspaces = 0;
        bool enter = false;
        input.consumeTextInput(chars, backspaces, enter);
        for (int i = 0; i < backspaces; i++) {
            if (!m_renameBuffer.empty()) m_renameBuffer.pop_back();
        }
        for (char c : chars) {
            if (c >= 32 && m_renameBuffer.size() < 24) m_renameBuffer += c;
        }
        if (enter) {
            m_renaming = false;
            input.textInputMode.store(false);
            return PauseResult::RenameWorld;
        }
        // Check for ESC
        auto presses = input.consumePresses();
        for (auto k : presses) {
            if (k == KeyPress::Back) {
                m_renaming = false;
                input.textInputMode.store(false);
                return PauseResult::None;
            }
        }
        return PauseResult::None;
    }

    // Handle Controls view
    if (m_view == PauseView::Controls) {
        auto presses = input.consumePresses();
        for (auto k : presses) {
            if (k == KeyPress::Back) {
                m_view = PauseView::MainMenu;
                m_selected = 0;
                return PauseResult::None;
            }
        }
        return PauseResult::None;
    }

    // Handle Settings view
    if (m_view == PauseView::Settings) {
        auto presses = input.consumePresses();
        for (auto k : presses) {
            if (k == KeyPress::Back) {
                m_view = PauseView::MainMenu;
                m_selected = 0;
                return PauseResult::None;
            } else if (k == KeyPress::Left) {
                // Decrease target FPS (min 20)
                if (targetFPS > 20) {
                    targetFPS--;
                }
            } else if (k == KeyPress::Right) {
                // Increase target FPS (max 100)
                if (targetFPS < 100) {
                    targetFPS++;
                }
            }
        }
        return PauseResult::None;
    }

    // Build items list dynamically
    std::vector<std::string> items;
    items.push_back("Resume");
    if (canRename) items.push_back("Rename World");
    items.push_back("Controls");
    items.push_back("Settings");
    items.push_back(isOnline ? "Disconnect" : "Go Multiplayer");
    items.push_back("Main Menu");
    int numItems = (int)items.size();
    if (m_selected >= numItems) m_selected = numItems - 1;

    auto presses = input.consumePresses();
    for (auto k : presses) {
        switch (k) {
        case KeyPress::Up:
            m_selected = (m_selected - 1 + numItems) % numItems;
            break;
        case KeyPress::Down:
            m_selected = (m_selected + 1) % numItems;
            break;
        case KeyPress::Confirm: {
            const std::string& sel = items[m_selected];
            if (sel == "Resume") return PauseResult::Resume;
            if (sel == "Rename World") {
                m_renaming = true;
                input.textInputMode.store(true);
                return PauseResult::None;
            }
            if (sel == "Controls") {
                m_view = PauseView::Controls;
                return PauseResult::None;
            }
            if (sel == "Settings") {
                m_view = PauseView::Settings;
                return PauseResult::None;
            }
            if (sel == "Disconnect") return PauseResult::Disconnect;
            if (sel == "Go Multiplayer") return PauseResult::GoMultiplayer;
            if (sel == "Main Menu") return PauseResult::ToMenu;
            break;
        }
        case KeyPress::Back:
            return PauseResult::Resume;
        default:
            break;
        }
    }
    return PauseResult::None;
}

void PauseOverlay::render(int screenW, int screenH, bool isOnline, bool canRename, int targetFPS) const {
    // Render Controls view
    if (m_view == PauseView::Controls) {
        renderControlsView(screenW, screenH);
        return;
    }

    // Render Settings view
    if (m_view == PauseView::Settings) {
        renderSettingsView(screenW, screenH, targetFPS);
        return;
    }

    // Build items list for main menu
    std::vector<std::string> items;
    items.push_back("Resume");
    if (canRename) items.push_back("Rename World");
    items.push_back("Controls");
    items.push_back("Settings");
    items.push_back(isOnline ? "Disconnect" : "Go Multiplayer");
    items.push_back("Main Menu");
    int numItems = (int)items.size();

    const int boxW = 30;
    int boxH = 6 + numItems + (m_renaming ? 2 : 0);

    int startX = (screenW - boxW) / 2;
    int startY = (screenH - boxH) / 2;
    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;

    const char* borderFg = "\033[38;2;180;170;140m";
    const char* borderBg = "\033[48;2;15;18;30m";
    const char* titleFg  = "\033[38;2;255;230;180m";
    const char* normalFg = "\033[38;2;120;120;130m";
    const char* selFg    = "\033[38;2;255;230;180m";
    const char* selBg    = "\033[48;2;30;35;50m";
    const char* hintFg   = "\033[38;2;60;65;80m";
    const char* inputFg  = "\033[38;2;255;230;180m";
    const char* inputBg  = "\033[48;2;25;28;40m";
    const char* reset    = "\033[0m";

    std::string out;
    out.reserve(4096);

    auto moveTo = [&](int row, int col) {
        char buf[24];
        int len = snprintf(buf, sizeof(buf), "\033[%d;%dH", row, col);
        out.append(buf, len);
    };

    auto drawBorderLine = [&](int row, char left, char fill, char right) {
        moveTo(row, startX + 1);
        out += borderFg; out += borderBg;
        out += left;
        for (int i = 0; i < boxW - 2; i++) out += fill;
        out += right;
        out += reset;
    };

    auto drawTextLine = [&](int row, const char* fg, const char* bg, const std::string& text) {
        moveTo(row, startX + 1);
        out += borderFg; out += borderBg; out += '|';
        out += fg; out += bg;
        int tlen = (int)text.size();
        int pad = (boxW - 2 - tlen) / 2;
        for (int i = 0; i < pad; i++) out += ' ';
        out += text;
        for (int i = 0; i < boxW - 2 - pad - tlen; i++) out += ' ';
        out += borderFg; out += borderBg; out += '|';
        out += reset;
    };

    auto drawEmptyLine = [&](int row) {
        moveTo(row, startX + 1);
        out += borderFg; out += borderBg; out += '|';
        for (int i = 0; i < boxW - 2; i++) out += ' ';
        out += '|'; out += reset;
    };

    int row = startY + 1;
    drawBorderLine(row++, '+', '-', '+');
    drawTextLine(row++, titleFg, borderBg, "PAUSED");
    drawEmptyLine(row++);

    // Menu items
    for (int i = 0; i < numItems; i++) {
        moveTo(row, startX + 1);
        out += borderFg; out += borderBg; out += '|';

        bool selected = (i == m_selected);
        out += (selected ? selFg : normalFg);
        out += (selected ? selBg : borderBg);

        const std::string& label = items[i];
        int labelLen = (int)label.size();
        int lpad = (boxW - 2 - labelLen - 2) / 2;
        for (int j = 0; j < lpad; j++) out += ' ';
        out += (selected ? "> " : "  ");
        out += label;
        int remaining = boxW - 2 - lpad - labelLen - 2;
        for (int j = 0; j < remaining; j++) out += ' ';

        out += borderFg; out += borderBg; out += '|'; out += reset;
        row++;
    }

    // Rename input field
    if (m_renaming) {
        drawEmptyLine(row++);
        moveTo(row, startX + 1);
        out += borderFg; out += borderBg; out += '|';
        out += inputFg; out += inputBg;
        std::string display = " " + m_renameBuffer + "_";
        int dlen = (int)display.size();
        out += display;
        for (int i = dlen; i < boxW - 2; i++) out += ' ';
        out += borderFg; out += borderBg; out += '|'; out += reset;
        row++;
    }

    drawEmptyLine(row++);
    drawTextLine(row++, hintFg, borderBg, "UP/DOWN  ENTER  ESC");
    drawBorderLine(row++, '+', '-', '+');

    fwrite(out.data(), 1, out.size(), stdout);
    fflush(stdout);
}

void PauseOverlay::renderControlsView(int screenW, int screenH) const {
    const int boxW = 50;
    const int boxH = 20;

    int startX = (screenW - boxW) / 2;
    int startY = (screenH - boxH) / 2;
    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;

    const char* borderFg = "\033[38;2;180;170;140m";
    const char* borderBg = "\033[48;2;15;18;30m";
    const char* titleFg  = "\033[38;2;255;230;180m";
    const char* normalFg = "\033[38;2;120;120;130m";
    const char* accentFg = "\033[38;2;140;180;120m";
    const char* hintFg   = "\033[38;2;60;65;80m";
    const char* reset    = "\033[0m";

    std::string out;
    out.reserve(4096);

    auto moveTo = [&](int row, int col) {
        char buf[24];
        int len = snprintf(buf, sizeof(buf), "\033[%d;%dH", row, col);
        out.append(buf, len);
    };

    auto drawBorderLine = [&](int row, char left, char fill, char right) {
        moveTo(row, startX + 1);
        out += borderFg; out += borderBg;
        out += left;
        for (int i = 0; i < boxW - 2; i++) out += fill;
        out += right;
        out += reset;
    };

    auto drawTextLine = [&](int row, const char* fg, const char* bg, const std::string& text, bool centered = true) {
        moveTo(row, startX + 1);
        out += borderFg; out += borderBg; out += '|';
        out += fg; out += bg;
        int tlen = (int)text.size();
        if (centered) {
            int pad = (boxW - 2 - tlen) / 2;
            for (int i = 0; i < pad; i++) out += ' ';
            out += text;
            for (int i = 0; i < boxW - 2 - pad - tlen; i++) out += ' ';
        } else {
            out += "  ";
            out += text;
            for (int i = tlen + 2; i < boxW - 2; i++) out += ' ';
        }
        out += borderFg; out += borderBg; out += '|';
        out += reset;
    };

    auto drawEmptyLine = [&](int row) {
        moveTo(row, startX + 1);
        out += borderFg; out += borderBg; out += '|';
        for (int i = 0; i < boxW - 2; i++) out += ' ';
        out += '|'; out += reset;
    };

    int row = startY + 1;
    drawBorderLine(row++, '+', '-', '+');
    drawTextLine(row++, titleFg, borderBg, "CONTROLS");
    drawEmptyLine(row++);
    
    // List controls
    drawTextLine(row++, accentFg, borderBg, "WASD", false);
    drawTextLine(row++, normalFg, borderBg, "  Move", false);
    drawEmptyLine(row++);
    drawTextLine(row++, accentFg, borderBg, "Mouse", false);
    drawTextLine(row++, normalFg, borderBg, "  Look around", false);
    drawEmptyLine(row++);
    drawTextLine(row++, accentFg, borderBg, "Left Click / F", false);
    drawTextLine(row++, normalFg, borderBg, "  Attack / Interact", false);
    drawEmptyLine(row++);
    drawTextLine(row++, accentFg, borderBg, "1-9", false);
    drawTextLine(row++, normalFg, borderBg, "  Select hotbar slot", false);
    drawEmptyLine(row++);
    drawTextLine(row++, accentFg, borderBg, "Q / E", false);
    drawTextLine(row++, normalFg, borderBg, "  Cycle hotbar", false);
    drawEmptyLine(row++);
    
    drawTextLine(row++, hintFg, borderBg, "Press ESC to go back");
    drawBorderLine(row++, '+', '-', '+');

    fwrite(out.data(), 1, out.size(), stdout);
    fflush(stdout);
}

void PauseOverlay::renderSettingsView(int screenW, int screenH, int targetFPS) const {
    const int boxW = 40;
    const int boxH = 10;

    int startX = (screenW - boxW) / 2;
    int startY = (screenH - boxH) / 2;
    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;

    const char* borderFg = "\033[38;2;180;170;140m";
    const char* borderBg = "\033[48;2;15;18;30m";
    const char* titleFg  = "\033[38;2;255;230;180m";
    const char* normalFg = "\033[38;2;120;120;130m";
    const char* hintFg   = "\033[38;2;60;65;80m";
    const char* reset    = "\033[0m";

    std::string out;
    out.reserve(4096);

    auto moveTo = [&](int row, int col) {
        char buf[24];
        int len = snprintf(buf, sizeof(buf), "\033[%d;%dH", row, col);
        out.append(buf, len);
    };

    auto drawBorderLine = [&](int row, char left, char fill, char right) {
        moveTo(row, startX + 1);
        out += borderFg; out += borderBg;
        out += left;
        for (int i = 0; i < boxW - 2; i++) out += fill;
        out += right;
        out += reset;
    };

    auto drawTextLine = [&](int row, const char* fg, const char* bg, const std::string& text) {
        moveTo(row, startX + 1);
        out += borderFg; out += borderBg; out += '|';
        out += fg; out += bg;
        int tlen = (int)text.size();
        int pad = (boxW - 2 - tlen) / 2;
        for (int i = 0; i < pad; i++) out += ' ';
        out += text;
        for (int i = 0; i < boxW - 2 - pad - tlen; i++) out += ' ';
        out += borderFg; out += borderBg; out += '|';
        out += reset;
    };

    auto drawEmptyLine = [&](int row) {
        moveTo(row, startX + 1);
        out += borderFg; out += borderBg; out += '|';
        for (int i = 0; i < boxW - 2; i++) out += ' ';
        out += '|'; out += reset;
    };

    int row = startY + 1;
    drawBorderLine(row++, '+', '-', '+');
    drawTextLine(row++, titleFg, borderBg, "SETTINGS");
    drawEmptyLine(row++);
    
    // Target FPS setting
    char fpsBuf[64];
    snprintf(fpsBuf, sizeof(fpsBuf), "< Target FPS: %d >", targetFPS);
    drawTextLine(row++, normalFg, borderBg, fpsBuf);
    drawTextLine(row++, hintFg, borderBg, "(20-100, default 30)");
    
    drawEmptyLine(row++);
    drawTextLine(row++, hintFg, borderBg, "LEFT/RIGHT  ESC");
    drawBorderLine(row++, '+', '-', '+');

    fwrite(out.data(), 1, out.size(), stdout);
    fflush(stdout);
}
