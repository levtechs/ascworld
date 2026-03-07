#pragma once
#include "ui/menu/menu_common.h"

class UsernameScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

    const std::string& username() const { return m_username; }
    void setUsername(const std::string& name) { m_username = name; }

private:
    std::string m_username;
    mutable int m_animFrame = 0;
    static constexpr int MAX_LEN = 16;
};
