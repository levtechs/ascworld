#pragma once
#include "input/input.h"
#include <string>

enum class DeathResult {
    None,       // Still showing death screen
    Respawn,    // Player chose to respawn
    ToMenu,     // Player chose to leave
};

class DeathOverlay {
public:
    DeathOverlay() = default;

    // Reset state when player dies
    void activate(const std::string& killerName = "");

    // Process input. Returns action taken.
    // Only allows selection after cooldown expires.
    DeathResult update(InputState& input, float dt);

    // Render the overlay popup centered on screen.
    void render(int screenW, int screenH) const;

    bool isActive() const { return m_active; }

private:
    bool m_active = false;
    float m_cooldown = 0.0f;
    int m_selected = 0;  // 0 = Respawn, 1 = Leave Game
    std::string m_killerName;
    static constexpr int NUM_ITEMS = 2;
    static constexpr float RESPAWN_COOLDOWN = 5.0f;
};
