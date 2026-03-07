#include "rendering/framebuffer.h"
#include "input/input.h"
#include "game/camera.h"
#include "game/player.h"
#include "rendering/renderer.h"
#include "world/world.h"
#include "game/save_system.h"
#include "network/lobby.h"
#include "network/net_common.h"
#include <csignal>

// Menu screens (one per file)
#include "ui/menu/menu_common.h"
#include "ui/menu/mode_select.h"
#include "ui/menu/main_menu.h"
#include "ui/menu/load_world.h"
#include "ui/menu/username.h"
#include "ui/menu/lobby.h"
#include "ui/menu/host_setup.h"
#include "ui/menu/join_password.h"
#include "ui/menu/connecting.h"
#include "ui/menu/generating.h"
#include "ui/menu/pause_overlay.h"

#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>

// Get terminal size in characters
static void getTerminalSize(int& width, int& height) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        width = ws.ws_col;
        height = ws.ws_row;
    } else {
        width = 80;
        height = 24;
    }
}

enum class GameState {
    ModeSelect,
    MainMenu,
    LoadWorld,
    UsernameEntry,
    OnlineLobby,
    HostSetup,
    JoinPassword,
    Connecting,
    Generating,
    Playing,
    OnlinePlaying,
    Paused,         // Offline paused — overlay over 3D world
    OnlinePaused,   // Online paused — overlay over 3D world, still sending state
};

// Auto-save current game (offline only)
static void autoSave(SaveSystem& saveSystem, const Player& player,
                     unsigned int seed, const std::string& worldName) {
    SaveData data;
    data.name = worldName;
    data.seed = seed;
    data.playerPos = player.position();
    data.playerYaw = player.yaw();
    data.playerPitch = player.pitch();
    data.timestamp = static_cast<int64_t>(std::time(nullptr));
    data.filename = SaveSystem::makeFilename(worldName);
    saveSystem.save(data);
}

// Load username from config file
static std::string loadUsername(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return "";
    char buf[64];
    if (fgets(buf, sizeof(buf), f)) {
        fclose(f);
        std::string s(buf);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
        if (s.size() > 16) s = s.substr(0, 16);
        return s;
    }
    fclose(f);
    return "";
}

// Save username to config file
static void saveUsername(const std::string& path, const std::string& name) {
    std::string dir = path.substr(0, path.rfind('/'));
    mkdir(dir.c_str(), 0755);
    FILE* f = fopen(path.c_str(), "w");
    if (f) {
        fprintf(f, "%s\n", name.c_str());
        fclose(f);
    }
}

// Clear the full screen
static void clearScreen() {
    printf("\033[2J");
    fflush(stdout);
}

int main() {
    // Install signal handlers for clean terminal restore on crash
    installSignalHandlers();

    // Ignore SIGPIPE — writing to a closed WebRTC/socket can generate this
    // and the default handler kills the process
    signal(SIGPIPE, SIG_IGN);

    bool forceUsernameEntry = false;
    {
        const char* env = getenv("ASCII3D_FORCE_USERNAME_ENTRY");
        if (env && env[0] != '\0' && env[0] != '0') {
            forceUsernameEntry = true;
        }
    }

    // Get terminal dimensions
    int screenW, screenH;
    getTerminalSize(screenW, screenH);
    screenH = screenH - 1;
    if (screenH < 10) screenH = 10;

    // Initialize core systems
    InputState inputState;
    Input input;
    input.m_sharedState = &inputState;
    input.startCapture();

    Player player;
    Camera camera;
    Renderer renderer;
    World world;
    SaveSystem saveSystem;
    Framebuffer fb(screenW, screenH);

    // Network
    Lobby lobby;
    bool lobbyInitialized = false;
    std::string configPath = std::string(getenv("HOME")) + "/.ascii3d/config";

    // Load saved username
    std::string savedUsername = loadUsername(configPath);
    if (forceUsernameEntry) {
        savedUsername.clear();
    }

    // ---- Create all screen objects ----
    ModeSelectScreen modeSelectScreen;
    MainMenuScreen mainMenuScreen;
    LoadWorldScreen loadWorldScreen;
    UsernameScreen usernameScreen;
    LobbyScreen lobbyScreen;
    HostSetupScreen hostSetupScreen;
    JoinPasswordScreen joinPasswordScreen;
    ConnectingScreen connectingScreen;
    PauseOverlay pauseOverlay;

    // Pre-populate
    if (!savedUsername.empty()) {
        usernameScreen.setUsername(savedUsername);
    }
    mainMenuScreen.setSaves(saveSystem.listSaves());
    loadWorldScreen.setSaves(saveSystem.listSaves());

    // Active screen pointer (points to one of the above, nullptr during gameplay)
    MenuScreen* activeScreen = &modeSelectScreen;

    // Clear screen initially
    clearScreen();
    printf("\033[?25l"); // hide cursor
    fflush(stdout);

    using Clock = std::chrono::high_resolution_clock;
    auto lastTime = Clock::now();
    const float targetFrameTime = 1.0f / 30.0f;

    GameState state = GameState::ModeSelect;
    int generationStep = 0;
    unsigned int worldSeed = 0;
    std::string currentWorldName;
    bool hasActiveGame = false;
    bool isOnlineGame = false;

    // Helper: transition to a new state with screen swap
    auto switchState = [&](GameState newState, MenuScreen* screen) {
        state = newState;
        activeScreen = screen;
        clearScreen();
    };

    // Helper: enter pause state (from Playing or OnlinePlaying)
    auto enterPause = [&]() {
        input.setMouseCapture(false);
        pauseOverlay.reset();
        if (isOnlineGame) {
            state = GameState::OnlinePaused;
        } else {
            state = GameState::Paused;
        }
    };

    // Helper: resume from pause
    auto resumePlay = [&]() {
        input.setMouseCapture(true);
        // Drain any stale presses accumulated while paused
        inputState.consumePresses();
        if (isOnlineGame) {
            state = GameState::OnlinePlaying;
        } else {
            state = GameState::Playing;
        }
    };

    auto refreshLobbyRooms = [&]() {
        auto rooms = lobby.refreshRooms();
        std::vector<RoomInfo> deduped;
        deduped.reserve(rooms.size());
        std::unordered_set<std::string> seen;
        for (const auto& r : rooms) {
            std::string key = r.roomId + "|" + r.name + "|" + r.hostName;
            if (seen.insert(key).second) {
                deduped.push_back(r);
            }
        }
        lobbyScreen.setRooms(deduped);
    };

    auto clearAndRefreshLobby = [&]() {
        lobbyScreen.clearRooms();
        refreshLobbyRooms();
    };

    while (!inputState.quit) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        if (dt > 0.1f) dt = 0.1f;

        // Poll input (also checks focus)
        input.poll(inputState);

        // Check terminal resize
        int newW, newH;
        getTerminalSize(newW, newH);
        newH = newH - 1;
        if (newH < 10) newH = 10;
        if (newW != screenW || newH != screenH) {
            screenW = newW;
            screenH = newH;
            fb = Framebuffer(screenW, screenH);
            clearScreen();
        }

        // Auto-pause on focus loss when in active gameplay
        if (inputState.focusLost.exchange(false, std::memory_order_relaxed)) {
            if (state == GameState::Playing || state == GameState::OnlinePlaying) {
                enterPause();
            }
        }

        switch (state) {
            // ---------------------------------------------------------------
            // MODE SELECT: Online vs Offline
            // ---------------------------------------------------------------
            case GameState::ModeSelect: {
                MenuResult result = activeScreen->update(inputState, screenW, screenH);
                if (result == MenuResult::OnlinePlay) {
                    if (usernameScreen.username().empty()) {
                        inputState.textInputMode.store(true, std::memory_order_relaxed);
                        switchState(GameState::UsernameEntry, &usernameScreen);
                    } else {
                        // Initialize lobby if needed
                        if (!lobbyInitialized) {
                            if (lobby.init()) {
                                lobbyInitialized = true;
                                lobby.setPlayerName(usernameScreen.username());
                                modeSelectScreen.clearError();
                            } else {
                                modeSelectScreen.setError("Could not initialize networking");
                                break;
                            }
                        }
                        lobbyScreen.setPlayerName(usernameScreen.username());
                        clearAndRefreshLobby();
                        switchState(GameState::OnlineLobby, &lobbyScreen);
                    }
                } else if (result == MenuResult::OfflinePlay) {
                    mainMenuScreen.setCanContinue(hasActiveGame && !isOnlineGame);
                    auto saves = saveSystem.listSaves();
                    mainMenuScreen.setSaves(saves);
                    mainMenuScreen.resetSelection();
                    switchState(GameState::MainMenu, &mainMenuScreen);
                } else if (result == MenuResult::Quit) {
                    if (hasActiveGame && !isOnlineGame) {
                        autoSave(saveSystem, player, worldSeed, currentWorldName);
                    }
                    inputState.quit = true;
                }
                if (state == GameState::ModeSelect) {
                    activeScreen->render(screenW, screenH);
                }
                break;
            }

            // ---------------------------------------------------------------
            // USERNAME ENTRY
            // ---------------------------------------------------------------
            case GameState::UsernameEntry: {
                MenuResult result = activeScreen->update(inputState, screenW, screenH);
                if (result == MenuResult::Back) {
                    inputState.textInputMode.store(false, std::memory_order_relaxed);
                    switchState(GameState::ModeSelect, &modeSelectScreen);
                } else if (result == MenuResult::OnlinePlay) {
                    // Username confirmed
                    inputState.textInputMode.store(false, std::memory_order_relaxed);
                    saveUsername(configPath, usernameScreen.username());
                    if (!lobbyInitialized) {
                        if (lobby.init()) {
                            lobbyInitialized = true;
                            lobby.setPlayerName(usernameScreen.username());
                            modeSelectScreen.clearError();
                        } else {
                            modeSelectScreen.setError("Could not initialize networking");
                            switchState(GameState::ModeSelect, &modeSelectScreen);
                            break;
                        }
                    } else {
                        lobby.setPlayerName(usernameScreen.username());
                    }
                    lobbyScreen.setPlayerName(usernameScreen.username());
                    clearAndRefreshLobby();
                    switchState(GameState::OnlineLobby, &lobbyScreen);
                }
                if (state == GameState::UsernameEntry) {
                    activeScreen->render(screenW, screenH);
                }
                break;
            }

            // ---------------------------------------------------------------
            // ONLINE LOBBY
            // ---------------------------------------------------------------
            case GameState::OnlineLobby: {
                // Check if lobby wants a room list refresh
                if (lobbyScreen.refreshRequested()) {
                    lobbyScreen.clearRefreshRequest();
                    clearAndRefreshLobby();
                }

                MenuResult result = activeScreen->update(inputState, screenW, screenH);
                if (result == MenuResult::HostGame) {
                    // Go to host setup screen
                    hostSetupScreen.reset();
                    inputState.textInputMode.store(true, std::memory_order_relaxed);
                    switchState(GameState::HostSetup, &hostSetupScreen);
                } else if (result == MenuResult::JoinGame) {
                    // User selected a room to join
                    if (lobbyScreen.joinRoomNeedsPassword()) {
                        joinPasswordScreen.reset();
                        inputState.textInputMode.store(true, std::memory_order_relaxed);
                        switchState(GameState::JoinPassword, &joinPasswordScreen);
                    } else {
                        // Join directly (no password)
                        bool joined = lobby.joinGame(lobbyScreen.joinRoomId());
                        if (joined) {
                            isOnlineGame = true;
                            connectingScreen.setStatus("Connecting to host...");
                            switchState(GameState::Connecting, &connectingScreen);
                        }
                    }
                } else if (result == MenuResult::Back) {
                    lobbyScreen.clearRooms();
                    switchState(GameState::ModeSelect, &modeSelectScreen);
                }
                if (state == GameState::OnlineLobby) {
                    activeScreen->render(screenW, screenH);
                }
                break;
            }

            // ---------------------------------------------------------------
            // HOST SETUP
            // ---------------------------------------------------------------
            case GameState::HostSetup: {
                MenuResult result = activeScreen->update(inputState, screenW, screenH);
                if (result == MenuResult::HostGame) {
                    inputState.textInputMode.store(false, std::memory_order_relaxed);
                    worldSeed = static_cast<unsigned int>(std::time(nullptr));
                    currentWorldName = "Online " + std::to_string(worldSeed % 10000);
                    std::string roomId = lobby.hostGame(
                        hostSetupScreen.roomName(), worldSeed,
                        hostSetupScreen.isPublic(), hostSetupScreen.password());
                    if (!roomId.empty()) {
                        isOnlineGame = true;
                        generationStep = 0;
                        state = GameState::Generating;
                        activeScreen = nullptr; // Generating uses static render
                        clearScreen();
                    } else {
                        // Failed to create room, go back to lobby
                        clearAndRefreshLobby();
                        switchState(GameState::OnlineLobby, &lobbyScreen);
                    }
                } else if (result == MenuResult::Back) {
                    inputState.textInputMode.store(false, std::memory_order_relaxed);
                    clearAndRefreshLobby();
                    switchState(GameState::OnlineLobby, &lobbyScreen);
                }
                if (state == GameState::HostSetup) {
                    activeScreen->render(screenW, screenH);
                }
                break;
            }

            // ---------------------------------------------------------------
            // JOIN PASSWORD
            // ---------------------------------------------------------------
            case GameState::JoinPassword: {
                MenuResult result = activeScreen->update(inputState, screenW, screenH);
                if (result == MenuResult::JoinGame) {
                    inputState.textInputMode.store(false, std::memory_order_relaxed);
                    bool joined = lobby.joinGame(lobbyScreen.joinRoomId(),
                                                 joinPasswordScreen.password());
                    if (joined) {
                        isOnlineGame = true;
                        connectingScreen.setStatus("Connecting to host...");
                        switchState(GameState::Connecting, &connectingScreen);
                    } else {
                        // Wrong password or failed — go back to lobby
                        clearAndRefreshLobby();
                        switchState(GameState::OnlineLobby, &lobbyScreen);
                    }
                } else if (result == MenuResult::Back) {
                    inputState.textInputMode.store(false, std::memory_order_relaxed);
                    switchState(GameState::OnlineLobby, &lobbyScreen);
                }
                if (state == GameState::JoinPassword) {
                    activeScreen->render(screenW, screenH);
                }
                break;
            }

            // ---------------------------------------------------------------
            // CONNECTING (client waiting for host)
            // ---------------------------------------------------------------
            case GameState::Connecting: {
                MenuResult result = activeScreen->update(inputState, screenW, screenH);
                if (result == MenuResult::Back || result == MenuResult::Disconnect) {
                    lobby.disconnect();
                    isOnlineGame = false;
                    clearAndRefreshLobby();
                    switchState(GameState::OnlineLobby, &lobbyScreen);
                    break;
                }

                // Poll connection progress
                auto clientSt = lobby.clientState();
                if (clientSt == ClientSession::State::Failed) {
                    connectingScreen.setStatus("Connection failed. Press ESC to go back.");
                } else if (lobby.hasWorldSeed()) {
                    worldSeed = lobby.worldSeed();
                    currentWorldName = "Online " + std::to_string(worldSeed % 10000);
                    generationStep = 0;
                    state = GameState::Generating;
                    activeScreen = nullptr;
                    clearScreen();
                    break;
                } else {
                    connectingScreen.setStatus("Connecting to host...");
                }

                activeScreen->render(screenW, screenH);
                break;
            }

            // ---------------------------------------------------------------
            // MAIN MENU (offline)
            // ---------------------------------------------------------------
            case GameState::MainMenu: {
                MenuResult result = activeScreen->update(inputState, screenW, screenH);
                if (result == MenuResult::NewGame) {
                    worldSeed = static_cast<unsigned int>(std::time(nullptr));
                    currentWorldName = "World " + std::to_string(worldSeed % 10000);
                    isOnlineGame = false;
                    generationStep = 0;
                    state = GameState::Generating;
                    activeScreen = nullptr;
                    clearScreen();
                } else if (result == MenuResult::LoadWorld) {
                    // Go to load world screen
                    loadWorldScreen.setSaves(saveSystem.listSaves());
                    switchState(GameState::LoadWorld, &loadWorldScreen);
                } else if (result == MenuResult::Continue) {
                    state = GameState::Playing;
                    activeScreen = nullptr;
                    input.setMouseCapture(true);
                    clearScreen();
                } else if (result == MenuResult::Quit) {
                    if (hasActiveGame) {
                        autoSave(saveSystem, player, worldSeed, currentWorldName);
                    }
                    inputState.quit = true;
                } else if (result == MenuResult::Back) {
                    switchState(GameState::ModeSelect, &modeSelectScreen);
                }
                if (state == GameState::MainMenu) {
                    activeScreen->render(screenW, screenH);
                }
                break;
            }

            // ---------------------------------------------------------------
            // LOAD WORLD
            // ---------------------------------------------------------------
            case GameState::LoadWorld: {
                MenuResult result = activeScreen->update(inputState, screenW, screenH);
                if (result == MenuResult::LoadWorld) {
                    const SaveData& save = loadWorldScreen.selectedSave();
                    worldSeed = save.seed;
                    currentWorldName = save.name;
                    isOnlineGame = false;
                    generationStep = 0;
                    state = GameState::Generating;
                    activeScreen = nullptr;
                    clearScreen();
                } else if (result == MenuResult::Back) {
                    mainMenuScreen.setCanContinue(hasActiveGame && !isOnlineGame);
                    mainMenuScreen.setSaves(saveSystem.listSaves());
                    switchState(GameState::MainMenu, &mainMenuScreen);
                }
                if (state == GameState::LoadWorld) {
                    activeScreen->render(screenW, screenH);
                }
                break;
            }

            // ---------------------------------------------------------------
            // GENERATING WORLD
            // ---------------------------------------------------------------
            case GameState::Generating: {
                int totalChunks = World::WORLD_CHUNKS * World::WORLD_CHUNKS;
                if (generationStep == 0) {
                    GeneratingScreen::render(screenW, screenH, 0, totalChunks);
                    generationStep = 1;
                } else if (generationStep == 1) {
                    GeneratingScreen::render(screenW, screenH, totalChunks / 2, totalChunks);
                    generationStep = 2;
                } else if (generationStep == 2) {
                    world.generate(worldSeed);
                    player.setTerrainQuery(&world);

                    if (isOnlineGame) {
                        float worldCenter = (World::WORLD_CHUNKS * World::CHUNK_SIZE) / 2.0f;
                        float spawnY = world.terrainHeightAt(worldCenter, worldCenter) + 2.0f;
                        player.setPosition({worldCenter, spawnY, worldCenter});
                        player.setYaw(0.0f);
                        player.setPitch(0.0f);
                    } else {
                        // Check if loading a save
                        SaveData loadedSave;
                        bool isLoadingSave = false;
                        auto saves = saveSystem.listSaves();
                        for (const auto& s : saves) {
                            if (s.name == currentWorldName && s.seed == worldSeed) {
                                loadedSave = s;
                                isLoadingSave = true;
                                break;
                            }
                        }
                        if (isLoadingSave) {
                            player.setPosition(loadedSave.playerPos);
                            player.setYaw(loadedSave.playerYaw);
                            player.setPitch(loadedSave.playerPitch);
                        } else {
                            float worldCenter = (World::WORLD_CHUNKS * World::CHUNK_SIZE) / 2.0f;
                            float spawnY = world.terrainHeightAt(worldCenter, worldCenter) + 2.0f;
                            player.setPosition({worldCenter, spawnY, worldCenter});
                            player.setYaw(0.0f);
                            player.setPitch(0.0f);
                        }
                    }

                    hasActiveGame = true;
                    GeneratingScreen::render(screenW, screenH, totalChunks, totalChunks);
                    generationStep = 3;
                } else {
                    // Generation done — start playing
                    input.setMouseCapture(true);
                    if (isOnlineGame) {
                        state = GameState::OnlinePlaying;
                    } else {
                        state = GameState::Playing;
                    }
                    activeScreen = nullptr;
                    clearScreen();
                }
                break;
            }

            // ---------------------------------------------------------------
            // PLAYING (offline)
            // ---------------------------------------------------------------
            case GameState::Playing: {
                // Check for ESC via press queue (one-shot, never missed)
                auto presses = inputState.consumePresses();
                for (auto k : presses) {
                    if (k == KeyPress::Back) {
                        enterPause();
                        break;
                    }
                }
                if (state != GameState::Playing) break; // Entered pause

                player.update(inputState, dt, world.colliders(), world.slopes());
                camera.setFromPlayer(player.eyePosition(), player.yaw(), player.pitch());

                fb.clear();
                renderer.render(world.objects(), world.lights(), camera, fb);
                fb.render();
                break;
            }

            // ---------------------------------------------------------------
            // ONLINE PLAYING
            // ---------------------------------------------------------------
            case GameState::OnlinePlaying: {
                // Check if host disconnected (client only)
                if (lobby.isClient() && lobby.clientState() == ClientSession::State::Failed) {
                    uint32_t seed = lobby.worldSeed();
                    if (seed == 0) seed = worldSeed;

                    // Attempt host reassignment by promoting this client to host.
                    std::string rehostName = "REHOST " + std::to_string(seed % 10000);
                    std::string newRoomId = lobby.hostGame(rehostName, seed, true, "");
                    if (!newRoomId.empty()) {
                        std::cerr << "[MAIN] Host disconnected; promoted local client to host in room " << newRoomId << std::endl;
                        worldSeed = seed;
                        currentWorldName = rehostName;
                        inputState.consumePresses();
                        break;
                    }

                    // Fallback: return to mode select if reassignment fails.
                    lobby.disconnect();
                    isOnlineGame = false;
                    hasActiveGame = false;
                    input.setMouseCapture(false);
                    inputState.consumePresses();
                    modeSelectScreen.setError("Host disconnected (rehost failed)");
                    switchState(GameState::ModeSelect, &modeSelectScreen);
                    break;
                }

                // Check for ESC via press queue (one-shot, never missed)
                auto pressesOnline = inputState.consumePresses();
                for (auto k : pressesOnline) {
                    if (k == KeyPress::Back) {
                        enterPause();
                        break;
                    }
                }
                if (state != GameState::OnlinePlaying) break; // Entered pause

                // Update local player
                player.update(inputState, dt, world.colliders(), world.slopes());
                camera.setFromPlayer(player.eyePosition(), player.yaw(), player.pitch());

                // Send local state to peers
                PlayerNetState localState;
                localState.id = lobby.localPeerId();
                localState.setPosition(player.position());
                localState.yaw = player.yaw();
                localState.pitch = player.pitch();
                localState.flags = 0;
                lobby.update(dt, localState);

                // Get remote players and build combined scene
                auto remotePlayers = lobby.getRemotePlayers();
                std::vector<SceneObject> allObjects(world.objects().begin(),
                                                     world.objects().end());
                for (auto* rp : remotePlayers) {
                    rp->update(dt);
                    allObjects.push_back(rp->bodyObject());
                    allObjects.push_back(rp->headObject());
                }

                fb.clear();
                renderer.render(allObjects, world.lights(), camera, fb);

                // HUD line
                int playerCount = static_cast<int>(remotePlayers.size()) + 1;
                char hudBuf[128];
                snprintf(hudBuf, sizeof(hudBuf),
                         "\033[%d;1H\033[36mOnline: %d player%s | %s\033[0m",
                         screenH + 1, playerCount,
                         playerCount == 1 ? "" : "s",
                         lobby.isHosting() ? "Hosting" : "Connected");
                printf("%s", hudBuf);

                fb.render();
                break;
            }

            // ---------------------------------------------------------------
            // PAUSED (offline) — overlay over frozen 3D world
            // ---------------------------------------------------------------
            case GameState::Paused: {
                PauseResult pr = pauseOverlay.update(inputState);
                if (pr == PauseResult::Resume) {
                    // Only resume if focused
                    if (inputState.focused.load(std::memory_order_relaxed)) {
                        resumePlay();
                    }
                } else if (pr == PauseResult::ToMenu) {
                    autoSave(saveSystem, player, worldSeed, currentWorldName);
                    mainMenuScreen.setCanContinue(true);
                    mainMenuScreen.setSaves(saveSystem.listSaves());
                    switchState(GameState::MainMenu, &mainMenuScreen);
                }

                if (state == GameState::Paused) {
                    // Re-render the last frame (world is frozen, no player update)
                    camera.setFromPlayer(player.eyePosition(), player.yaw(), player.pitch());
                    fb.clear();
                    renderer.render(world.objects(), world.lights(), camera, fb);
                    fb.render();
                    // Draw pause overlay on top
                    pauseOverlay.render(screenW, screenH);
                }
                break;
            }

            // ---------------------------------------------------------------
            // ONLINE PAUSED — overlay over 3D world, still in multiplayer
            // ---------------------------------------------------------------
            case GameState::OnlinePaused: {
                // Check if host disconnected (client only)
                if (lobby.isClient() && lobby.clientState() == ClientSession::State::Failed) {
                    uint32_t seed = lobby.worldSeed();
                    if (seed == 0) seed = worldSeed;

                    std::string rehostName = "REHOST " + std::to_string(seed % 10000);
                    std::string newRoomId = lobby.hostGame(rehostName, seed, true, "");
                    if (!newRoomId.empty()) {
                        std::cerr << "[MAIN] Host disconnected during pause; promoted local client to host in room " << newRoomId << std::endl;
                        worldSeed = seed;
                        currentWorldName = rehostName;
                        inputState.consumePresses();
                        break;
                    }

                    lobby.disconnect();
                    isOnlineGame = false;
                    hasActiveGame = false;
                    inputState.consumePresses();
                    modeSelectScreen.setError("Host disconnected (rehost failed)");
                    switchState(GameState::ModeSelect, &modeSelectScreen);
                    break;
                }

                PauseResult pr = pauseOverlay.update(inputState);
                if (pr == PauseResult::Resume) {
                    // Only resume if focused
                    if (inputState.focused.load(std::memory_order_relaxed)) {
                        resumePlay();
                    }
                } else if (pr == PauseResult::ToMenu) {
                    // Disconnect from multiplayer and go to mode select
                    lobby.disconnect();
                    isOnlineGame = false;
                    hasActiveGame = false;
                    switchState(GameState::ModeSelect, &modeSelectScreen);
                }

                if (state == GameState::OnlinePaused) {
                    // Keep sending position to peers (player is still in world)
                    PlayerNetState localState;
                    localState.id = lobby.localPeerId();
                    localState.setPosition(player.position());
                    localState.yaw = player.yaw();
                    localState.pitch = player.pitch();
                    localState.flags = 0;
                    lobby.update(dt, localState);

                    // Render world with remote players (but no local movement)
                    auto remotePlayers = lobby.getRemotePlayers();
                    std::vector<SceneObject> allObjects(world.objects().begin(),
                                                         world.objects().end());
                    for (auto* rp : remotePlayers) {
                        rp->update(dt);
                        allObjects.push_back(rp->bodyObject());
                        allObjects.push_back(rp->headObject());
                    }

                    camera.setFromPlayer(player.eyePosition(), player.yaw(), player.pitch());
                    fb.clear();
                    renderer.render(allObjects, world.lights(), camera, fb);

                    // HUD line
                    int playerCount = static_cast<int>(remotePlayers.size()) + 1;
                    char hudBuf[128];
                    snprintf(hudBuf, sizeof(hudBuf),
                             "\033[%d;1H\033[36mOnline: %d player%s | %s | PAUSED\033[0m",
                             screenH + 1, playerCount,
                             playerCount == 1 ? "" : "s",
                             lobby.isHosting() ? "Hosting" : "Connected");
                    printf("%s", hudBuf);

                    fb.render();
                    // Draw pause overlay on top
                    pauseOverlay.render(screenW, screenH);
                }
                break;
            }
        }

        // Frame timing
        auto frameEnd = Clock::now();
        float elapsed = std::chrono::duration<float>(frameEnd - now).count();
        float sleepTime = targetFrameTime - elapsed;
        if (sleepTime > 0.0f) {
            std::this_thread::sleep_for(
                std::chrono::microseconds(static_cast<int>(sleepTime * 1000000)));
        }
    }

    // Disconnect if still online
    if (lobby.isOnline()) {
        lobby.disconnect();
    }

    // Cleanup
    printf("\033[2J\033[H");
    printf("\033[?25h"); // ensure cursor visible
    printf("Goodbye!\n");

    return 0;
}
