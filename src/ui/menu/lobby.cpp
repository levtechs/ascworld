#include "ui/menu/lobby.h"
#include <cstring>

void LobbyScreen::setRooms(const std::vector<RoomInfo>& rooms) {
    m_rooms = rooms;
    if (m_roomSelected >= (int)m_rooms.size())
        m_roomSelected = m_rooms.empty() ? 0 : (int)m_rooms.size() - 1;
}

void LobbyScreen::clearRooms() {
    m_rooms.clear();
    m_roomSelected = 0;
    m_focus = 1;        // default to room list
    m_buttonIdx = 0;
    m_joinRoomId.clear();
    m_joinNeedsPassword = false;
    m_refreshRequested = false;
}

MenuResult LobbyScreen::update(InputState& input, int /*screenW*/, int /*screenH*/) {
    auto p = consumePressFlags(input);

    if (p.back) return MenuResult::Back;

    if (m_focus == 0) {
        // Username line selected
        if (p.down) {
            m_focus = 1;  // move to room list
        }
        if (p.confirm) {
            return MenuResult::ChangeUsername;
        }
    } else if (m_focus == 1) {
        // Room list
        int count = (int)m_rooms.size();
        if (p.up) {
            if (count > 0 && m_roomSelected > 0) {
                m_roomSelected--;
            } else {
                // At top of list (or empty) — move to username
                m_focus = 0;
            }
        }
        if (p.down) {
            if (count > 0 && m_roomSelected < count - 1) {
                m_roomSelected++;
            } else {
                // At bottom of list (or empty) — move to button bar
                m_focus = 2;
                m_buttonIdx = 0;
            }
        }
        // Enter on room = join
        if (p.confirm && count > 0 && m_roomSelected < count) {
            const RoomInfo& room = m_rooms[m_roomSelected];
            m_joinRoomId = room.roomId;
            m_joinNeedsPassword = room.hasPassword;
            return MenuResult::JoinGame;
        }
    } else {
        // Button bar (focus == 2)
        if (p.up) {
            m_focus = 1;  // back to room list
        }
        if (p.left) { m_buttonIdx--; if (m_buttonIdx < 0) m_buttonIdx = 3; }
        if (p.right) { m_buttonIdx++; if (m_buttonIdx > 3) m_buttonIdx = 0; }

        if (p.confirm) {
            switch (m_buttonIdx) {
                case 0: return MenuResult::HostGame;
                case 1: return MenuResult::CustomizeChar;
                case 2: m_refreshRequested = true; return MenuResult::None;
                case 3: return MenuResult::Back;
            }
        }
    }

    return MenuResult::None;
}

void LobbyScreen::render(int screenW, int screenH) const {
    std::string output;
    beginOutput(output, screenW, screenH);

    int headerY = 2;
    int usernameY = 4;
    int columnHeaderY = 6;
    int listStartY = 7;
    int maxVisible = screenH - 14;
    if (maxVisible < 3) maxVisible = 3;

    int totalRooms = (int)m_rooms.size();

    int scrollOffset = 0;
    if (totalRooms > maxVisible) {
        scrollOffset = m_roomSelected - maxVisible / 2;
        if (scrollOffset < 0) scrollOffset = 0;
        if (scrollOffset > totalRooms - maxVisible) scrollOffset = totalRooms - maxVisible;
    }

    const char* buttonLabels[] = { "[Host Game]", "[Customize]", "[Refresh]", "[Back]" };
    int buttonCount = 4;
    int buttonY = screenH - 5;

    for (int y = 0; y < screenH; y++) {
        bool rendered = false;

        if (y == headerY) {
            renderCenteredText(output, "=== Online Lobby ===", screenW, TITLE_FG, BG);
            rendered = true;
        }

        if (y == usernameY) {
            bool userSel = (m_focus == 0);
            std::string userLine = "Player: " + m_playerName;
            if (userSel) userLine += "  [Change]";
            renderCenteredText(output, userLine.c_str(), screenW,
                               userSel ? SELECTED_FG : ACCENT_FG,
                               userSel ? SELECTED_BG : BG);
            rendered = true;
        }

        if (y == columnHeaderY) {
            std::string header = "  Name                 Host           Players  Status  ";
            int padLeft = (screenW - (int)header.size()) / 2;
            if (padLeft < 0) padLeft = 0;

            output += DETAIL_FG;
            output += BG;
            for (int x = 0; x < padLeft && x < screenW; x++) output += ' ';
            for (int x = 0; x < (int)header.size() && (padLeft + x) < screenW; x++) output += header[x];
            for (int x = padLeft + (int)header.size(); x < screenW; x++) output += ' ';
            rendered = true;
        }

        // Room list
        int listIdx = y - listStartY;
        if (y >= listStartY && listIdx < maxVisible && (listIdx + scrollOffset) < totalRooms) {
            int roomIdx = listIdx + scrollOffset;
            const RoomInfo& room = m_rooms[roomIdx];
            bool sel = (m_focus == 1 && roomIdx == m_roomSelected);

            std::string nameStr = room.name;
            if ((int)nameStr.size() > 20) nameStr = nameStr.substr(0, 17) + "...";
            while ((int)nameStr.size() < 20) nameStr += ' ';

            std::string hostStr = room.hostName;
            if ((int)hostStr.size() > 14) hostStr = hostStr.substr(0, 11) + "...";
            while ((int)hostStr.size() < 14) hostStr += ' ';

            char playersBuf[16];
            snprintf(playersBuf, sizeof(playersBuf), "%d/%d", room.playerCount, room.maxPlayers);
            std::string playersStr = playersBuf;
            while ((int)playersStr.size() < 8) playersStr += ' ';

            std::string statusStr = room.hasPassword ? "Locked" : "Open";

            std::string line = sel ? "> " : "  ";
            line += nameStr + " " + hostStr + " " + playersStr + " " + statusStr;
            while ((int)line.size() < 56) line += ' ';

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

        // "No rooms" message
        if (y == listStartY + 1 && totalRooms == 0) {
            renderCenteredText(output, "No rooms available", screenW, DETAIL_FG, BG);
            rendered = true;
        }

        // Action buttons
        if (y == buttonY) {
            std::string buttonLine;
            for (int b = 0; b < buttonCount; b++) {
                if (b > 0) buttonLine += "   ";
                buttonLine += buttonLabels[b];
            }

            int padLeft = (screenW - (int)buttonLine.size()) / 2;
            if (padLeft < 0) padLeft = 0;

            output += BG;
            for (int x = 0; x < padLeft && x < screenW; x++) output += ' ';

            int col = padLeft;
            for (int b = 0; b < buttonCount; b++) {
                if (b > 0) { output += BG; output += NORMAL_FG; output += "   "; col += 3; }
                bool btnSel = (m_focus == 2 && b == m_buttonIdx);
                output += (btnSel ? SELECTED_FG : NORMAL_FG);
                output += (btnSel ? SELECTED_BG : BG);
                output += buttonLabels[b];
                col += (int)strlen(buttonLabels[b]);
            }

            output += BG;
            for (int x = col; x < screenW; x++) output += ' ';
            rendered = true;
        }

        if (y == screenH - 2) {
            renderCenteredText(output, "UP/DOWN: Navigate | LEFT/RIGHT: Buttons | ENTER: Select | ESC: Back",
                             screenW, HINT_FG, BG);
            rendered = true;
        }

        if (!rendered) renderEmptyLine(output, screenW, BG);
        endLine(output, y, screenH);
    }

    output += "\033[0m";
    flushOutput(output);
}
