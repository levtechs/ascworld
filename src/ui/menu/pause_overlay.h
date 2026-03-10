#pragma once
#include "input/input.h"
#include <string>

// Lightweight pause overlay — renders a small popup over the 3D world.
// Does NOT extend MenuScreen (it's drawn after the framebuffer, not full-screen).
enum class PauseResult {
    None,       // Still showing pause menu
    Resume,     // User chose Continue
    ToMenu,     // User chose Main Menu
    GoMultiplayer, // User chose "Go Multiplayer" (if offline)
    Disconnect,    // User chose "Disconnect" (if online)
    RenameWorld,   // Host chose "Rename World"
    ShowControls,  // User chose "Controls"
    ShowSettings,  // User chose "Settings"
};

enum class PauseView {
    MainMenu,
    Controls,
    Settings,
};

class PauseOverlay {
public:
    PauseOverlay() = default;

    PauseResult update(InputState& input, bool isOnline, bool canRename, int& targetFPS);
    void render(int screenW, int screenH, bool isOnline, bool canRename, int targetFPS) const;

    void reset() { m_selected = 0; m_view = PauseView::MainMenu; m_settingsSelected = 0; }

    // For rename: get/set the editing state
    bool isRenaming() const { return m_renaming; }
    void startRename(const std::string& current) { m_renaming = true; m_renameBuffer = current; }
    void stopRename() { m_renaming = false; }
    const std::string& renameBuffer() const { return m_renameBuffer; }

private:
    void renderControlsView(int screenW, int screenH) const;
    void renderSettingsView(int screenW, int screenH, int targetFPS) const;

    int m_selected = 0;
    int m_settingsSelected = 0;  // Which setting item is selected in settings view
    bool m_renaming = false;
    std::string m_renameBuffer;
    PauseView m_view = PauseView::MainMenu;
};
