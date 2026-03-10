#pragma once
#include "ui/menu/menu_common.h"
#include "game/save_system.h"
#include <vector>

class LoadWorldScreen : public MenuScreen {
public:
    MenuResult update(InputState& input, int screenW, int screenH) override;
    void render(int screenW, int screenH) const override;

    void setSaves(const std::vector<SaveSummary>& saves);
    const SaveSummary& selectedSave() const { return m_saves[m_selected - 1]; }
    bool isNewGameSelected() const { return m_selected == 0; }
    bool empty() const { return m_saves.empty(); }

private:
    static std::string formatTimestamp(int64_t timestamp);
    std::vector<SaveSummary> m_saves;
    int m_selected = 0;
};
