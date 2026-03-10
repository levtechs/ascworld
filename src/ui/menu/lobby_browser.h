#pragma once
#include "ui/menu/menu_common.h"
#include "game/networking.h"
#include <vector>

class LobbyBrowserScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

    void setLobbies(const std::vector<LobbyInfo>& lobbies) { 
        m_lobbies = lobbies; 
        if (m_selected >= (int)m_lobbies.size()) m_selected = (int)m_lobbies.size() - 1;
        if (m_selected < 0) m_selected = 0;
    }
    std::string getSelectedUUID() const { 
        if (m_selected >= 0 && m_selected < (int)m_lobbies.size()) return m_lobbies[m_selected].uuid;
        return "";
    }

private:
    int m_selected = 0;
    std::vector<LobbyInfo> m_lobbies;
};
