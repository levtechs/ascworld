#pragma once
#include "input/input.h"
#include <string>

// Lightweight pause overlay — renders a small popup over the 3D world.
// Does NOT extend MenuScreen (it's drawn after the framebuffer, not full-screen).
enum class PauseResult {
    None,       // Still showing pause menu
    Resume,     // User chose Continue
    ToMenu,     // User chose Main Menu
};

class PauseOverlay {
public:
    PauseOverlay() = default;

    // Process input. Returns action taken.
    PauseResult update(InputState& input);

    // Render the overlay popup centered on screen.
    // Call AFTER framebuffer.render() so it draws on top.
    void render(int screenW, int screenH) const;

    // Reset selection to "Continue" (call when opening the overlay)
    void reset() { m_selected = 0; }

private:
    int m_selected = 0;  // 0 = Continue, 1 = Main Menu
    static constexpr int NUM_ITEMS = 2;
};
