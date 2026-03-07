#include "ui/hud/death_overlay.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

void DeathOverlay::activate(const std::string& killerName) {
    m_active = true;
    m_cooldown = RESPAWN_COOLDOWN;
    m_selected = 0;
    m_killerName = killerName;
}

DeathResult DeathOverlay::update(InputState& input, float dt) {
    if (!m_active) return DeathResult::None;

    if (m_cooldown > 0.0f) {
        m_cooldown -= dt;
        if (m_cooldown < 0.0f) m_cooldown = 0.0f;
    }

    auto presses = input.consumePresses();
    for (auto k : presses) {
        switch (k) {
        case KeyPress::Up:
        case KeyPress::Down:
            m_selected = (m_selected + 1) % NUM_ITEMS;
            break;
        case KeyPress::Confirm:
            if (m_selected == 0) {
                if (m_cooldown > 0.0f) break; // Respawn gated by cooldown
                m_active = false;
                return DeathResult::Respawn;
            }
            if (m_selected == 1) {
                // Leave Game always available immediately
                m_active = false;
                return DeathResult::ToMenu;
            }
            break;
        default:
            break;
        }
    }
    return DeathResult::None;
}

void DeathOverlay::render(int screenW, int screenH) const {
    if (!m_active) return;

    const int boxW = 28;
    const int boxH = 9;

    int startX = (screenW - boxW) / 2;
    int startY = (screenH - boxH) / 2;
    if (startX < 0) startX = 0;
    if (startY < 0) startY = 0;

    const char* borderFg = "\033[38;2;140;50;50m";
    const char* borderBg = "\033[48;2;20;8;8m";
    const char* titleFg  = "\033[38;2;255;80;70m";
    const char* normalFg = "\033[38;2;120;90;90m";
    const char* selFg    = "\033[38;2;255;200;180m";
    const char* selBg    = "\033[48;2;50;15;15m";
    const char* dimFg    = "\033[38;2;80;55;55m";
    const char* hintFg   = "\033[38;2;80;50;50m";
    const char* reset    = "\033[0m";

    std::string out;
    out.reserve(2048);

    auto moveTo = [&](int row, int col) {
        char buf[24];
        int len = snprintf(buf, sizeof(buf), "\033[%d;%dH", row, col);
        out.append(buf, len);
    };

    auto fillLine = [&](const char* fg, const char* bg, const char* text) {
        out += borderFg;
        out += borderBg;
        out += '|';
        out += fg;
        out += bg;
        int textLen = static_cast<int>(strlen(text));
        int pad = (boxW - 2 - textLen) / 2;
        for (int i = 0; i < pad; i++) out += ' ';
        out += text;
        for (int i = 0; i < boxW - 2 - pad - textLen; i++) out += ' ';
        out += borderFg;
        out += borderBg;
        out += '|';
        out += reset;
    };

    int row = startY + 1;

    // Top border
    moveTo(row, startX + 1);
    out += borderFg;
    out += borderBg;
    out += '+';
    for (int i = 0; i < boxW - 2; i++) out += '-';
    out += '+';
    out += reset;
    row++;

    // Title
    moveTo(row, startX + 1);
    fillLine(titleFg, borderBg, "YOU DIED");
    row++;

    // Killed by line
    moveTo(row, startX + 1);
    if (!m_killerName.empty()) {
        std::string killedBy = "by " + m_killerName;
        if (killedBy.size() > static_cast<size_t>(boxW - 4))
            killedBy = killedBy.substr(0, boxW - 4);
        fillLine(normalFg, borderBg, killedBy.c_str());
    } else {
        fillLine(borderFg, borderBg, "");
    }
    row++;

    // Menu items
    const char* items[NUM_ITEMS] = { "Respawn", "Leave Game" };

    for (int i = 0; i < NUM_ITEMS; i++) {
        moveTo(row, startX + 1);
        out += borderFg;
        out += borderBg;
        out += '|';

        bool selected = (i == m_selected);
        // Only Respawn (index 0) is gated by cooldown; Leave Game always available
        bool itemEnabled = (i == 1) || (m_cooldown <= 0.0f);
        const char* fg = itemEnabled ? (selected ? selFg : normalFg) : dimFg;
        const char* bg = (itemEnabled && selected) ? selBg : borderBg;
        out += fg;
        out += bg;

        const char* label = items[i];
        int labelLen = static_cast<int>(strlen(label));
        int lpad = (boxW - 2 - labelLen - 4) / 2 + 2;
        for (int j = 0; j < lpad - 2; j++) out += ' ';
        if (selected && itemEnabled) {
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

    // Empty
    moveTo(row, startX + 1);
    fillLine(borderFg, borderBg, "");
    row++;

    // Cooldown or hint
    moveTo(row, startX + 1);
    if (m_cooldown > 0.0f) {
        char cdBuf[32];
        snprintf(cdBuf, sizeof(cdBuf), "Respawn in %.1fs", m_cooldown);
        fillLine(hintFg, borderBg, cdBuf);
    } else {
        fillLine(hintFg, borderBg, "UP/DOWN  ENTER");
    }
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
