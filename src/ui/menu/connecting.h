#pragma once
#include "ui/menu/menu_common.h"

class ConnectingScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

    void setStatus(const std::string& s) { m_status = s; }

private:
    std::string m_status = "Connecting...";
    mutable int m_animFrame = 0;
};
