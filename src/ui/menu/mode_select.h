#pragma once
#include "ui/menu/menu_common.h"
#include <string>

class ModeSelectScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

    void setError(const std::string& err) { m_error = err; }
    void clearError() { m_error.clear(); }
private:
    int m_selected = 0; // 0=Online, 1=Offline
    std::string m_error;
};
