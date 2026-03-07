#pragma once
#include "ui/menu/menu_common.h"
#include "game/character_appearance.h"

class CustomizeScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

    void setAppearance(const CharacterAppearance& a) { m_appearance = a; }
    const CharacterAppearance& appearance() const { return m_appearance; }

    void reset() { m_field = 0; }

private:
    CharacterAppearance m_appearance;
    int m_field = 0;  // 0=color, 1=design, 2=Done button
};
