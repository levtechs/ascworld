#pragma once
#include "ui/menu/menu_common.h"
#include "game/character_appearance.h"
#include <string>

class CustomizeScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

    void setAppearance(const CharacterAppearance& a) { m_appearance = a; }
    const CharacterAppearance& appearance() const { return m_appearance; }

    void setUsername(const std::string& name) { m_username = name; }
    const std::string& username() const { return m_username; }

    void reset() { m_field = 0; m_editing = false; }

private:
    CharacterAppearance m_appearance;
    std::string m_username;
    int m_field = 0;  // 0=username, 1=color, 2=design, 3=Done
    bool m_editing = false; // true when typing username
};
