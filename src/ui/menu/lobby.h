#pragma once
#include "ui/menu/menu_common.h"
#include "network/net_common.h"
#include <vector>

class LobbyScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

    void setRooms(const std::vector<RoomInfo>& rooms);
    void clearRooms();
    void setPlayerName(const std::string& name) { m_playerName = name; }

    const std::string& joinRoomId() const { return m_joinRoomId; }
    bool joinRoomNeedsPassword() const { return m_joinNeedsPassword; }

    // Signal that refresh was requested (main.cpp checks this)
    bool refreshRequested() const { return m_refreshRequested; }
    void clearRefreshRequest() { m_refreshRequested = false; }

private:
    std::vector<RoomInfo> m_rooms;
    std::string m_playerName;
    int m_roomSelected = 0;
    int m_focus = 1;        // 0=username, 1=room list, 2=buttons
    int m_buttonIdx = 0;    // 0=Host, 1=Customize, 2=Refresh, 3=Back

    std::string m_joinRoomId;
    bool m_joinNeedsPassword = false;
    bool m_refreshRequested = false;
};
