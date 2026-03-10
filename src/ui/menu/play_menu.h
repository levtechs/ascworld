#pragma once
#include "ui/menu/menu_common.h"
#include <vector>

class PlayMenuScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

private:
    int m_selected = 0;
    const std::vector<std::pair<std::string, MenuResult>> m_items = {
        {"Single Player", MenuResult::OfflinePlay},
        {"Multiplayer", MenuResult::OnlinePlay},
        {"Back", MenuResult::Back}
    };
};
