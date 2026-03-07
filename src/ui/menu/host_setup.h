#pragma once
#include "ui/menu/menu_common.h"

class HostSetupScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

    const std::string& roomName() const { return m_roomName; }
    bool isPublic() const { return m_isPublic; }
    const std::string& password() const { return m_password; }

    void reset() { m_roomName.clear(); m_isPublic = true; m_password.clear(); m_field = 0; }

private:
    std::string m_roomName;
    bool m_isPublic = true;
    std::string m_password;
    int m_field = 0; // 0=name, 1=visibility, 2=password(if private), last=start
    mutable int m_animFrame = 0;
};
