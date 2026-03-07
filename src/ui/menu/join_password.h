#pragma once
#include "ui/menu/menu_common.h"

class JoinPasswordScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

    const std::string& password() const { return m_password; }
    void reset() { m_password.clear(); }

private:
    std::string m_password;
    mutable int m_animFrame = 0;
};
