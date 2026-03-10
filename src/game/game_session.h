#pragma once
#include "rendering/framebuffer.h"
#include "input/input.h"
#include "game/camera.h"
#include "game/player.h"
#include "game/item.h"
#include "game/inventory.h"
#include "game/weapon_meshes.h"
#include "game/world_state.h"
#include "game/combat.h"
#include "rendering/renderer.h"
#include "world/world.h"
#include "game/save_system.h"
#include "game/root_state.h"
#include "game/networking.h"
#include "game/state_sync.h"
#include "game/character_appearance.h"
#include "ui/menu/menu_common.h"
#include "ui/menu/main_menu.h"
#include "ui/menu/load_world.h"
#include "ui/menu/play_menu.h"
#include "ui/menu/lobby_browser.h"
#include "ui/menu/pause_overlay.h"
#include "ui/menu/generating.h"
#include "ui/menu/customize_screen.h"
#include "ui/hud/hud.h"
#include "ui/hud/kill_feed.h"
#include "ui/hud/death_overlay.h"
#include <string>
#include <chrono>

// GameSession owns all game state and runs the game loop.
// main.cpp creates one of these and calls run().
class GameSession {
public:
    GameSession(const std::string& configDir, const std::string& clientUUID,
                const std::string& savedUsername, bool forceUsernameEntry);

    // Run the game loop until quit. Blocks until the game exits.
    void run();

private:
    enum class GameState {
        MainMenu,
        PlayMenu,
        LobbyBrowser,
        LoadWorld,
        Generating,
        Connecting,
        Playing,
        Paused,
        Dead,
        Customize
    };

    // State transitions
    void switchState(GameState newState, MenuScreen* screen);
    void enterPause();
    void resumePlay();
    void enterDeath();

    // Per-state update methods
    void updateMainMenu();
    void updatePlayMenu();
    void updateLobbyBrowser(float dt);
    void updateLoadWorld();
    void updateConnecting(float dt);
    void updateCustomize();
    void updateGenerating();
    void updatePlaying(float dt);
    void updatePaused(float dt);
    void updateDead(float dt);

    // Rendering helpers (shared between Playing/Paused/Dead)
    void renderScene(const std::unordered_set<std::string>& activePeers);
    void renderHeldWeapon();

    // Autosave helper
    void doAutoSave();

    // Sync local Player object from a PlayerState (used by HostAuthority callbacks)
    void syncFromPlayerState(const PlayerState& ps);

    // --- All game state ---
    std::string m_configDir;
    std::string m_configPath;
    std::string m_clientUUID;
    std::string m_savedUsername;
    CharacterAppearance m_localAppearance;

    // Screen
    int m_screenW = 0, m_screenH = 0;

    // Core game objects
    Player m_player;
    Camera m_camera;
    Renderer m_renderer;
    World m_world;
    SaveSystem m_saveSystem;
    Framebuffer m_fb;
    WeaponMeshes m_weaponMeshes;
    RootState m_activeState;
    WorldState m_worldState;
    CombatSystem m_combat;
    KillFeed m_killFeed;
    std::string m_lastDamagerName;

    // Input
    InputState m_inputState;
    Input m_input;

    // Networking
    NetworkingManager m_netManager;

    // UI screens
    MainMenuScreen m_mainMenuScreen;
    PlayMenuScreen m_playMenuScreen;
    LobbyBrowserScreen m_lobbyBrowserScreen;
    LoadWorldScreen m_loadWorldScreen;
    GeneratingScreen m_generatingScreen;
    CustomizeScreen m_customizeScreen;
    PauseOverlay m_pauseOverlay;
    DeathOverlay m_deathOverlay;

    // State machine
    GameState m_state = GameState::MainMenu;
    MenuScreen* m_activeScreen = nullptr;

    // Timing
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point m_lastTime;
    int m_targetFPS = 30;
    int m_lastSavedFPS = 30;
    float m_targetFrameTime = 1.0f / 30.0f;

    // Game flags
    int m_generationStep = 0;
    bool m_hasActiveGame = false;
    bool m_isDiscoverable = false;  // replaces isOnlineGame - means lobby is public
    float m_timeSinceAutoSave = 0.0f;
    static constexpr float AUTO_SAVE_INTERVAL = 30.0f;
    float m_lobbyRefreshTimer = 0.0f;
};
