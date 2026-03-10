#pragma once
#include "ui/menu/menu_common.h"
#include "game/save_system.h"
#include <vector>

class MainMenuScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

    void setCanContinue(bool v) { m_canContinue = v; }
    void setSaves(const std::vector<SaveSummary>& saves) { m_saves = saves; }
    bool hasSaves() const { return !m_saves.empty(); }
    void resetSelection() { m_selected = 0; }

private:
    std::vector<std::string> getItems() const;
    int m_selected = 0;
    bool m_canContinue = false;
    std::vector<SaveSummary> m_saves;
};
