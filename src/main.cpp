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
#include "network/lobby.h"
#include "network/net_common.h"
#include "ui/hud/hud.h"
#include "ui/hud/kill_feed.h"
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
#include "ui/menu/customize_screen.h"
#include "ui/hud/death_overlay.h"
#include "game/character_appearance.h"

#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

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

static bool projectToScreen(const Camera& camera,
                            const Vec3& worldPos,
                            int screenW,
                            int screenH,
                            int& outX,
                            int& outY) {
    Mat4 view = camera.viewMatrix();
    Mat4 proj = camera.projectionMatrix(screenW, screenH);
    Vec4 clip = proj * (view * Vec4(worldPos, 1.0f));
    if (clip.w <= 0.001f) return false;

    Vec3 ndc = clip.perspectiveDivide();
    if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
    if (ndc.x < -1.2f || ndc.x > 1.2f || ndc.y < -1.2f || ndc.y > 1.2f) return false;

    outX = static_cast<int>((ndc.x * 0.5f + 0.5f) * (screenW - 1));
    outY = static_cast<int>((1.0f - (ndc.y * 0.5f + 0.5f)) * (screenH - 1));
    return outX >= 0 && outX < screenW && outY >= 0 && outY < screenH;
}

static void renderRemoteNameLabels(const Camera& camera,
                                   const std::vector<RemotePlayer*>& remotePlayers,
                                   int screenW,
                                   int screenH) {
    for (auto* rp : remotePlayers) {
        if (!rp) continue;

        Vec3 labelPos = rp->position() + Vec3(0.0f, 1.95f, 0.0f);
        int sx = 0;
        int sy = 0;
        if (!projectToScreen(camera, labelPos, screenW, screenH, sx, sy)) continue;

        const std::string& name = rp->name();
        if (name.empty()) continue;

        std::string shown = name;
        if (shown.size() > 16) shown = shown.substr(0, 16);

        int startX = sx - static_cast<int>(shown.size()) / 2;
        if (startX < 0) startX = 0;
        if (startX + static_cast<int>(shown.size()) > screenW) {
            startX = screenW - static_cast<int>(shown.size());
        }
        if (startX < 0 || sy < 0 || sy >= screenH) continue;

        printf("\033[%d;%dH\033[38;2;255;245;190m%s\033[0m",
               sy + 1, startX + 1, shown.c_str());
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
    Dead,           // Offline dead — overlay with respawn timer
    OnlineDead,     // Online dead — overlay with respawn timer, still syncing
    CustomizingChar, // Character customization screen (from online lobby)
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

    // Weapon / combat systems
    WeaponMeshes weaponMeshes;
    WorldState worldState;
    CombatSystem combat;
    float gameTime = 0.0f;
    float localHealth = 100.0f;
    int localKills = 0;
    int localDeaths = 0;
    bool isDead = false;
    std::unordered_map<uint8_t, float> playerHealth;
    std::unordered_set<uint8_t> m_lastDamageTargets; // tracks who we recently damaged for kill detection

    // Track last damage source per target for kill feed (client-side)
    struct DamageInfo { uint8_t sourceId; EntityType weapon; };
    std::unordered_map<uint8_t, DamageInfo> m_lastDamageSource;
    KillFeed killFeed;

    // Track who last damaged us (for death screen)
    std::string lastDamagerName;
    EntityType lastDamagerWeapon = EntityType::None;

    // Character appearance
    CharacterAppearance localAppearance;

    // Network
    Lobby lobby;
    bool lobbyInitialized = false;

    // Wire up network world events
    lobby.setWorldEventCallback([&](const uint8_t* data, size_t len) {
        if (len < 1) return;
        NetMsgType msgType = getMessageType(data, len);
        if (msgType == NetMsgType::EntitySpawn) {
            EntitySpawnMsg msg;
            if (EntitySpawnMsg::deserialize(data, len, msg)) {
                msg.entity.spawnTime = gameTime;
                worldState.spawnEntityWithId(msg.entity);
            }
        } else if (msgType == NetMsgType::EntityRemove) {
            EntityRemoveMsg msg;
            if (EntityRemoveMsg::deserialize(data, len, msg)) {
                worldState.removeEntity(msg.entityId);
            }
        } else if (msgType == NetMsgType::DamageEvent) {
            DamageEventMsg msg;
            if (DamageEventMsg::deserialize(data, len, msg)) {
                if (msg.targetId == lobby.localPeerId()) {
                    localHealth = std::max(0.0f, localHealth - msg.amount);
                    // Track who last damaged us (for death screen)
                    lastDamagerWeapon = msg.weaponType;
                    // Look up killer name
                    if (msg.sourceId == lobby.localPeerId()) {
                        lastDamagerName = "yourself";
                    } else {
                        lastDamagerName = "";
                        auto rps = lobby.getRemotePlayers();
                        for (auto* rp : rps) {
                            if (rp->peerId() == msg.sourceId) {
                                lastDamagerName = rp->name();
                                break;
                            }
                        }
                        if (lastDamagerName.empty()) {
                            lastDamagerName = "Player " + std::to_string(msg.sourceId);
                        }
                    }
                }
                // Remember last damage source for kill tracking + kill feed
                if (msg.sourceId == lobby.localPeerId() && msg.targetId != lobby.localPeerId()) {
                    m_lastDamageTargets.insert(msg.targetId);
                }
                // Store damage info for kill feed (all damage, not just ours)
                m_lastDamageSource[msg.targetId] = DamageInfo{msg.sourceId, msg.weaponType};
            }
        } else if (msgType == NetMsgType::HealthState) {
            HealthStateMsg msg;
            if (HealthStateMsg::deserialize(data, len, msg)) {
                float prevHp = 100.0f;
                auto it = playerHealth.find(msg.playerId);
                if (it != playerHealth.end()) prevHp = it->second;
                playerHealth[msg.playerId] = msg.health;
                if (msg.playerId == lobby.localPeerId()) {
                    localHealth = msg.health;
                }
                // Track kills: target HP went from >0 to <=0
                if (!lobby.isHosting() && prevHp > 0.0f && msg.health <= 0.0f) {
                    // Increment local kills if we killed them
                    if (m_lastDamageTargets.count(msg.playerId)) {
                        localKills++;
                    }
                    // Add to kill feed
                    auto dit = m_lastDamageSource.find(msg.playerId);
                    if (dit != m_lastDamageSource.end()) {
                        // Look up names inline (can't use getPlayerName here)
                        auto lookupName = [&](uint8_t pid) -> std::string {
                            if (pid == lobby.localPeerId()) return lobby.playerName();
                            auto rps = lobby.getRemotePlayers();
                            for (auto* rp : rps) {
                                if (rp->peerId() == pid) return rp->name();
                            }
                            return "Player " + std::to_string(pid);
                        };
                        killFeed.addKill(lookupName(dit->second.sourceId),
                                         lookupName(msg.playerId),
                                         dit->second.weapon);
                    }
                }
                m_lastDamageTargets.erase(msg.playerId);
                m_lastDamageSource.erase(msg.playerId);
            }
        }
    });

    auto ensureHealthEntry = [&](uint8_t peerId) {
        auto it = playerHealth.find(peerId);
        if (it == playerHealth.end()) {
            playerHealth[peerId] = 100.0f;
        }
    };

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
    DeathOverlay deathOverlay;
    CustomizeScreen customizeScreen;

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

    // Look up player name by peerId
    auto getPlayerName = [&](uint8_t peerId) -> std::string {
        if (isOnlineGame && peerId == lobby.localPeerId()) {
            return usernameScreen.username();
        }
        if (isOnlineGame) {
            auto remotePlayers = lobby.getRemotePlayers();
            for (auto* rp : remotePlayers) {
                if (rp->peerId() == peerId) return rp->name();
            }
        }
        return "Player " + std::to_string(peerId);
    };

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

    auto applyAuthoritativeDamage = [&](const std::vector<DamageEvent>& events) {
        if (!lobby.isHosting()) return;
        for (const auto& ev : events) {
            ensureHealthEntry(ev.targetId);
            float& hp = playerHealth[ev.targetId];
            if (hp <= 0.0f) continue; // already dead, skip
            float prevHp = hp;
            hp = std::max(0.0f, hp - ev.amount);
            if (ev.targetId == lobby.localPeerId()) {
                localHealth = hp;
            }

            DamageEventMsg dmsg;
            dmsg.sourceId = ev.sourceId;
            dmsg.targetId = ev.targetId;
            dmsg.amount = ev.amount;
            dmsg.weaponType = ev.weaponType;
            lobby.sendReliable(dmsg.serialize());

            HealthStateMsg hmsg;
            hmsg.playerId = ev.targetId;
            hmsg.health = hp;
            lobby.sendReliable(hmsg.serialize());

            // Track kills: if target just died
            if (prevHp > 0.0f && hp <= 0.0f) {
                if (ev.sourceId == lobby.localPeerId()) {
                    localKills++;
                }
                // Add to kill feed
                killFeed.addKill(getPlayerName(ev.sourceId),
                                 getPlayerName(ev.targetId),
                                 ev.weaponType);
            }

            // Track last damager for our own death screen
            if (ev.targetId == lobby.localPeerId()) {
                lastDamagerName = getPlayerName(ev.sourceId);
                lastDamagerWeapon = ev.weaponType;
            }
        }
    };

    // Helper: enter death state
    auto enterDeath = [&]() {
        input.setMouseCapture(false);
        deathOverlay.activate(lastDamagerName);
        isDead = true;
        localDeaths++;
        if (isOnlineGame) {
            state = GameState::OnlineDead;
        } else {
            state = GameState::Dead;
        }
    };

    // Helper: respawn player
    auto respawnPlayer = [&]() {
        float worldCenter = (World::WORLD_CHUNKS * World::CHUNK_SIZE) / 2.0f;
        float spawnY = world.terrainHeightAt(worldCenter, worldCenter) + 2.0f;
        player.setPosition({worldCenter, spawnY, worldCenter});
        player.setYaw(0.0f);
        player.setPitch(0.0f);
        player.inventory() = Inventory();
        player.inventory().addItem(ItemType::Saber);
        player.inventory().addItem(ItemType::Laser);
        player.inventory().addItem(ItemType::Flashbang, 3);
        localHealth = 100.0f;
        isDead = false;
        combat = CombatSystem();
        // Reset health in playerHealth map so host knows we're alive again
        if (isOnlineGame) {
            playerHealth[lobby.localPeerId()] = 100.0f;
            // Broadcast our health reset to all peers
            HealthStateMsg hmsg;
            hmsg.playerId = lobby.localPeerId();
            hmsg.health = 100.0f;
            lobby.sendReliable(hmsg.serialize());
        } else {
            playerHealth[0] = 100.0f;
        }
        input.setMouseCapture(true);
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
                                lobby.setAppearance(localAppearance.serialize());
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
                            lobby.setAppearance(localAppearance.serialize());
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
                } else if (result == MenuResult::ChangeUsername) {
                    inputState.textInputMode.store(true, std::memory_order_relaxed);
                    switchState(GameState::UsernameEntry, &usernameScreen);
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
                } else if (result == MenuResult::CustomizeChar) {
                    customizeScreen.setAppearance(localAppearance);
                    customizeScreen.reset();
                    switchState(GameState::CustomizingChar, &customizeScreen);
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
            // CHARACTER CUSTOMIZATION
            // ---------------------------------------------------------------
            case GameState::CustomizingChar: {
                MenuResult result = activeScreen->update(inputState, screenW, screenH);
                if (result == MenuResult::Back) {
                    // Save chosen appearance
                    localAppearance = customizeScreen.appearance();
                    lobby.setAppearance(localAppearance.serialize());
                    clearAndRefreshLobby();
                    switchState(GameState::OnlineLobby, &lobbyScreen);
                }
                if (state == GameState::CustomizingChar) {
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

                    // Give player default loadout
                    player.inventory() = Inventory(); // reset
                    player.inventory().addItem(ItemType::Saber);
                    player.inventory().addItem(ItemType::Laser);
                    player.inventory().addItem(ItemType::Flashbang, 3);

                    // Reset combat and world state for new game
                    combat = CombatSystem();
                    worldState.clear();
                    gameTime = 0.0f;
                    localHealth = 100.0f;
                    playerHealth.clear();

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
                gameTime += dt;

                // Check for ESC and weapon keys via press queue
                auto presses = inputState.consumePresses();
                bool attackPressed = false;
                for (auto k : presses) {
                    if (k == KeyPress::Back) {
                        enterPause();
                        break;
                    }
                    // Weapon cycling
                    else if (k == KeyPress::KeyQ) player.inventory().cycleLeft();
                    else if (k == KeyPress::KeyE) player.inventory().cycleRight();
                    // Number keys for direct slot selection
                    else if (k >= KeyPress::Key1 && k <= KeyPress::Key9) {
                        int slot = static_cast<int>(k) - static_cast<int>(KeyPress::Key1);
                        player.inventory().selectSlot(slot);
                    }
                    // Drop active item
                    else if (k == KeyPress::KeyF) {
                        ItemStack dropped = player.inventory().dropActive();
                        if (!dropped.empty()) {
                            float cy = std::cos(player.yaw());
                            float sy = std::sin(player.yaw());
                            Vec3 dropPos = player.position() + Vec3(-sy * 1.0f, 0.5f, -cy * 1.0f);
                            worldState.dropItem(dropped.type, dropped.count, dropPos, gameTime, 0);
                        }
                    }
                    // Pickup nearby item
                    else if (k == KeyPress::KeyG) {
                        auto* nearby = worldState.findNearestPickable(player.position());
                        if (nearby) {
                            worldState.pickupItem(nearby->id, player.inventory());
                        }
                    }
                    // Attack
                    else if (k == KeyPress::MouseLeft) {
                        attackPressed = true;
                    }
                }
                if (state != GameState::Playing) break; // Entered pause

                // Update systems
                player.update(inputState, dt, world.colliders(), world.slopes());
                auto spawned = combat.update(player, attackPressed, dt, 0);
                for (auto& e : spawned) {
                    e.spawnTime = gameTime;
                    worldState.spawnEntity(e);
                }

                std::vector<PlayerSnapshot> snaps;
                snaps.push_back(PlayerSnapshot{0, player.position(), player.radius(), player.height()});
                std::vector<DamageEvent> damageEvents;
                worldState.update(dt, gameTime, world.colliders(), true, snaps, damageEvents);
                for (const auto& ev : damageEvents) {
                    if (ev.targetId == 0) {
                        localHealth = std::max(0.0f, localHealth - ev.amount);
                    }
                }

                // Check death
                if (localHealth <= 0.0f && !isDead) {
                    enterDeath();
                    break;
                }

                camera.setFromPlayer(player.eyePosition(), player.yaw(), player.pitch());

                // Build scene: world + dynamic entities
                std::vector<SceneObject> allObjects(world.objects().begin(),
                                                     world.objects().end());
                worldState.gatherSceneObjects(allObjects, weaponMeshes, gameTime);

                fb.clear();
                renderer.render(allObjects, world.lights(), world.lightGrid(),
                                camera, fb);

                // Render beam/effect overlays (separate pass to avoid occlusion)
                {
                    std::vector<SceneObject> overlayObjs;
                    worldState.gatherOverlayObjects(overlayObjs, weaponMeshes, gameTime);
                    if (!overlayObjs.empty()) {
                        renderer.render(overlayObjs, world.lights(), world.lightGrid(),
                                        camera, fb);
                    }
                }

                // Render held weapon (first-person, after scene but before fb output)
                ItemType activeWeapon = player.inventory().activeItem().type;
                if (activeWeapon != ItemType::None) {
                    float atkProgress = combat.isAttacking() ? combat.attackProgress() : -1.0f;
                    auto heldObjs = weaponMeshes.getHeldObjects(
                        activeWeapon, player.eyePosition(), player.yaw(), player.pitch(), atkProgress);
                    for (const auto& obj : heldObjs) {
                        std::vector<SceneObject> heldVec = {obj};
                        renderer.render(heldVec, world.lights(), world.lightGrid(),
                                        camera, fb);
                    }
                }

                fb.render();

                // HUD overlay
                auto* nearbyItem = worldState.findNearestPickable(player.position());
                renderHUD(screenW, screenH, player, gameTime, nearbyItem, localHealth, 100.0f,
                          localKills, localDeaths);
                killFeed.update(dt);
                killFeed.render(screenW, screenH);
                fflush(stdout);
                break;
            }

            // ---------------------------------------------------------------
            // ONLINE PLAYING
            // ---------------------------------------------------------------
            case GameState::OnlinePlaying: {
                gameTime += dt;

                // Check if host disconnected (client only)
                if (lobby.isClient() && lobby.clientState() == ClientSession::State::Failed) {
                    uint32_t seed = lobby.worldSeed();
                    if (seed == 0) seed = worldSeed;

                    // Attempt host reassignment by promoting this client to host.
                    // Keep the original room name.
                    std::string rehostName = lobby.roomName();
                    if (rehostName.empty()) {
                        rehostName = "Online " + std::to_string(seed % 10000);
                    }
                    std::string newRoomId = lobby.hostGame(rehostName, seed, true, "");;
                    if (!newRoomId.empty()) {
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

                // Check for ESC and weapon keys via press queue
                auto pressesOnline = inputState.consumePresses();
                bool attackPressedOnline = false;
                for (auto k : pressesOnline) {
                    if (k == KeyPress::Back) {
                        enterPause();
                        break;
                    }
                    else if (k == KeyPress::KeyQ) player.inventory().cycleLeft();
                    else if (k == KeyPress::KeyE) player.inventory().cycleRight();
                    else if (k >= KeyPress::Key1 && k <= KeyPress::Key9) {
                        int slot = static_cast<int>(k) - static_cast<int>(KeyPress::Key1);
                        player.inventory().selectSlot(slot);
                    }
                    else if (k == KeyPress::KeyF) {
                        ItemStack dropped = player.inventory().dropActive();
                        if (!dropped.empty()) {
                            float cy = std::cos(player.yaw());
                            float sy = std::sin(player.yaw());
                            Vec3 dropPos = player.position() + Vec3(-sy * 1.0f, 0.5f, -cy * 1.0f);
                            WorldEntity ent;
                            ent.type = EntityType::DroppedItem;
                            ent.ownerId = lobby.localPeerId();
                            ent.position = dropPos;
                            ent.spawnTime = gameTime;
                            DroppedItemData d;
                            d.itemType = dropped.type;
                            d.count = static_cast<uint8_t>(std::max(1, dropped.count));
                            ent.data = d;
                            uint16_t id = worldState.spawnEntity(ent);
                            const WorldEntity* spawnedEnt = worldState.findEntity(id);
                            if (spawnedEnt) {
                                EntitySpawnMsg msg;
                                msg.entity = *spawnedEnt;
                                lobby.sendReliable(msg.serialize());
                            }
                        }
                    }
                    else if (k == KeyPress::KeyG) {
                        auto* nearby = worldState.findNearestPickable(player.position());
                        if (nearby) {
                            uint16_t pickedId = nearby->id;
                            if (worldState.pickupItem(pickedId, player.inventory())) {
                                EntityRemoveMsg rm;
                                rm.entityId = pickedId;
                                lobby.sendReliable(rm.serialize());
                            }
                        }
                    }
                    else if (k == KeyPress::MouseLeft) {
                        attackPressedOnline = true;
                    }
                }
                if (state != GameState::OnlinePlaying) break; // Entered pause

                // Update local player
                player.update(inputState, dt, world.colliders(), world.slopes());
                auto spawnedOnline = combat.update(player, attackPressedOnline, dt, lobby.localPeerId());
                for (auto& e : spawnedOnline) {
                    e.spawnTime = gameTime;
                    uint16_t id = worldState.spawnEntity(e);
                    const WorldEntity* spawnedEnt = worldState.findEntity(id);
                    if (spawnedEnt) {
                        EntitySpawnMsg msg;
                        msg.entity = *spawnedEnt;
                        lobby.sendReliable(msg.serialize());
                    }
                }

                camera.setFromPlayer(player.eyePosition(), player.yaw(), player.pitch());

                // Send local state to peers (with weapon flags)
                PlayerNetState localState;
                localState.id = lobby.localPeerId();
                localState.setPosition(player.position());
                localState.yaw = player.yaw();
                localState.pitch = player.pitch();
                localState.flags = encodeWeaponFlags(
                    0,  // base flags (crouch/onGround not yet exposed by Player)
                    player.inventory().activeItem().type,
                    combat.isAttacking(),
                    isDead);
                lobby.update(dt, localState);

                // Get remote players and build combined scene
                auto remotePlayers = lobby.getRemotePlayers();
                for (auto* rp : remotePlayers) rp->update(dt);

                std::vector<PlayerSnapshot> snaps;
                snaps.push_back(PlayerSnapshot{lobby.localPeerId(), player.position(), player.radius(), player.height()});
                for (auto* rp : remotePlayers) {
                    snaps.push_back(PlayerSnapshot{rp->peerId(), rp->position(), 0.3f, 1.8f});
                    ensureHealthEntry(rp->peerId());
                }

                std::vector<DamageEvent> damageEvents;
                worldState.update(dt, gameTime, world.colliders(), lobby.isHosting(), snaps, damageEvents);
                if (lobby.isHosting()) {
                    applyAuthoritativeDamage(damageEvents);
                }

                // Check death
                if (localHealth <= 0.0f && !isDead) {
                    enterDeath();
                    break;
                }

                std::vector<SceneObject> allObjects(world.objects().begin(),
                                                     world.objects().end());
                worldState.gatherSceneObjects(allObjects, weaponMeshes, gameTime);

                for (auto* rp : remotePlayers) {
                    allObjects.push_back(rp->bodyObject());
                    allObjects.push_back(rp->headObject());
                    // Add remote player's weapon mesh
                    if (rp->activeWeapon() != ItemType::None) {
                        float rpAtk = rp->isAttacking() ? 0.5f : -1.0f;
                        auto wpnObjs = weaponMeshes.getThirdPersonObjects(
                            rp->activeWeapon(), rp->position(), rp->yaw(), rpAtk);
                        for (auto& o : wpnObjs) allObjects.push_back(std::move(o));
                    }
                }

                fb.clear();
                renderer.render(allObjects, world.lights(), world.lightGrid(),
                                camera, fb);

                // Render beam/effect overlays
                {
                    std::vector<SceneObject> overlayObjs;
                    worldState.gatherOverlayObjects(overlayObjs, weaponMeshes, gameTime);
                    if (!overlayObjs.empty()) {
                        renderer.render(overlayObjs, world.lights(), world.lightGrid(),
                                        camera, fb);
                    }
                }

                // Render held weapon (first-person)
                ItemType activeWeaponOnline = player.inventory().activeItem().type;
                if (activeWeaponOnline != ItemType::None) {
                    float atkProgress = combat.isAttacking() ? combat.attackProgress() : -1.0f;
                    auto heldObjs = weaponMeshes.getHeldObjects(
                        activeWeaponOnline, player.eyePosition(), player.yaw(), player.pitch(), atkProgress);
                    for (const auto& obj : heldObjs) {
                        std::vector<SceneObject> heldVec = {obj};
                        renderer.render(heldVec, world.lights(), world.lightGrid(),
                                        camera, fb);
                    }
                }

                fb.render();
                renderRemoteNameLabels(camera, remotePlayers, screenW, screenH);

                // HUD overlay (weapons + pickup prompt)
                auto* nearbyItemOnline = worldState.findNearestPickable(player.position());
                renderHUD(screenW, screenH, player, gameTime, nearbyItemOnline, localHealth, 100.0f,
                          localKills, localDeaths);

                // Network status line
                int playerCount = static_cast<int>(remotePlayers.size()) + 1;
                char hudBuf[128];
                snprintf(hudBuf, sizeof(hudBuf),
                         "\033[%d;1H\033[36mOnline: %d player%s | %s\033[0m",
                         screenH + 1, playerCount,
                         playerCount == 1 ? "" : "s",
                         lobby.isHosting() ? "Hosting" : "Connected");
                printf("%s", hudBuf);
                killFeed.update(dt);
                killFeed.render(screenW, screenH);
                fflush(stdout);
                break;
            }

            // ---------------------------------------------------------------
            // PAUSED (offline) — overlay over frozen 3D world
            // ---------------------------------------------------------------
            case GameState::Paused: {
                // Check if player died while paused (e.g. from a projectile)
                if (localHealth <= 0.0f && !isDead) {
                    enterDeath();
                    break;
                }

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

                    // Build scene with world-state entities
                    std::vector<SceneObject> allObjects(world.objects().begin(),
                                                         world.objects().end());
                    worldState.gatherSceneObjects(allObjects, weaponMeshes, gameTime);

                    fb.clear();
                    renderer.render(allObjects, world.lights(), world.lightGrid(),
                                    camera, fb);

                    // Render beam/effect overlays
                    {
                        std::vector<SceneObject> overlayObjs;
                        worldState.gatherOverlayObjects(overlayObjs, weaponMeshes, gameTime);
                        if (!overlayObjs.empty()) {
                            renderer.render(overlayObjs, world.lights(), world.lightGrid(),
                                            camera, fb);
                        }
                    }

                    // Render held weapon
                    ItemType activeWeaponPaused = player.inventory().activeItem().type;
                    if (activeWeaponPaused != ItemType::None) {
                        auto heldObjs = weaponMeshes.getHeldObjects(
                            activeWeaponPaused, player.eyePosition(), player.yaw(), player.pitch(), -1.0f);
                        for (const auto& obj : heldObjs) {
                            std::vector<SceneObject> heldVec = {obj};
                            renderer.render(heldVec, world.lights(), world.lightGrid(),
                                            camera, fb);
                        }
                    }

                    fb.render();

                    // HUD overlay
                    auto* nearbyPaused = worldState.findNearestPickable(player.position());
                    renderHUD(screenW, screenH, player, gameTime, nearbyPaused, localHealth, 100.0f,
                              localKills, localDeaths);
                    killFeed.update(dt);
                    killFeed.render(screenW, screenH);

                    // Draw pause overlay on top
                    pauseOverlay.render(screenW, screenH);
                    fflush(stdout);
                }
                break;
            }

            // ---------------------------------------------------------------
            // ONLINE PAUSED — overlay over 3D world, still in multiplayer
            // ---------------------------------------------------------------
            case GameState::OnlinePaused: {
                // Check if player died while paused
                if (localHealth <= 0.0f && !isDead) {
                    enterDeath();
                    break;
                }

                // Check if host disconnected (client only)
                if (lobby.isClient() && lobby.clientState() == ClientSession::State::Failed) {
                    uint32_t seed = lobby.worldSeed();
                    if (seed == 0) seed = worldSeed;

                    // Keep the original room name.
                    std::string rehostName = lobby.roomName();
                    if (rehostName.empty()) {
                        rehostName = "Online " + std::to_string(seed % 10000);
                    }
                    std::string newRoomId = lobby.hostGame(rehostName, seed, true, "");
                    if (!newRoomId.empty()) {
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
                    gameTime += dt;

                    // Keep sending position to peers with weapon state
                    PlayerNetState localState;
                    localState.id = lobby.localPeerId();
                    localState.setPosition(player.position());
                    localState.yaw = player.yaw();
                    localState.pitch = player.pitch();
                    localState.flags = encodeWeaponFlags(
                        0, player.inventory().activeItem().type, false, isDead);
                    lobby.update(dt, localState);

                    // Render world with remote players + dropped items
                    auto remotePlayers = lobby.getRemotePlayers();
                    for (auto* rp : remotePlayers) rp->update(dt);

                    // Keep world state ticking (projectiles, beams, explosions, damage)
                    std::vector<PlayerSnapshot> snaps;
                    snaps.push_back(PlayerSnapshot{lobby.localPeerId(), player.position(), player.radius(), player.height()});
                    for (auto* rp : remotePlayers) {
                        snaps.push_back(PlayerSnapshot{rp->peerId(), rp->position(), 0.3f, 1.8f});
                        ensureHealthEntry(rp->peerId());
                    }

                    std::vector<DamageEvent> damageEvents;
                    worldState.update(dt, gameTime, world.colliders(), lobby.isHosting(), snaps, damageEvents);
                    if (lobby.isHosting()) {
                        applyAuthoritativeDamage(damageEvents);
                    }

                    std::vector<SceneObject> allObjects(world.objects().begin(),
                                                         world.objects().end());
                    worldState.gatherSceneObjects(allObjects, weaponMeshes, gameTime);

                    for (auto* rp : remotePlayers) {
                        allObjects.push_back(rp->bodyObject());
                        allObjects.push_back(rp->headObject());
                        if (rp->activeWeapon() != ItemType::None) {
                            float rpAtk = rp->isAttacking() ? 0.5f : -1.0f;
                            auto wpnObjs = weaponMeshes.getThirdPersonObjects(
                                rp->activeWeapon(), rp->position(), rp->yaw(), rpAtk);
                            for (auto& o : wpnObjs) allObjects.push_back(std::move(o));
                        }
                    }

                    camera.setFromPlayer(player.eyePosition(), player.yaw(), player.pitch());
                    fb.clear();
                    renderer.render(allObjects, world.lights(), world.lightGrid(),
                                    camera, fb);

                    // Render beam/effect overlays
                    {
                        std::vector<SceneObject> overlayObjs;
                        worldState.gatherOverlayObjects(overlayObjs, weaponMeshes, gameTime);
                        if (!overlayObjs.empty()) {
                            renderer.render(overlayObjs, world.lights(), world.lightGrid(),
                                            camera, fb);
                        }
                    }

                    // Render held weapon
                    ItemType activeWeaponOP = player.inventory().activeItem().type;
                    if (activeWeaponOP != ItemType::None) {
                        auto heldObjs = weaponMeshes.getHeldObjects(
                            activeWeaponOP, player.eyePosition(), player.yaw(), player.pitch(), -1.0f);
                        for (const auto& obj : heldObjs) {
                            std::vector<SceneObject> heldVec = {obj};
                            renderer.render(heldVec, world.lights(), world.lightGrid(),
                                            camera, fb);
                        }
                    }

                    fb.render();
                    renderRemoteNameLabels(camera, remotePlayers, screenW, screenH);

                    // HUD overlay
                    auto* nearbyOP = worldState.findNearestPickable(player.position());
                    renderHUD(screenW, screenH, player, gameTime, nearbyOP, localHealth, 100.0f,
                              localKills, localDeaths);

                    // Network status line
                    int playerCount = static_cast<int>(remotePlayers.size()) + 1;
                    char hudBuf[128];
                    snprintf(hudBuf, sizeof(hudBuf),
                             "\033[%d;1H\033[36mOnline: %d player%s | %s | PAUSED\033[0m",
                             screenH + 1, playerCount,
                             playerCount == 1 ? "" : "s",
                             lobby.isHosting() ? "Hosting" : "Connected");
                    printf("%s", hudBuf);
                    killFeed.update(dt);
                    killFeed.render(screenW, screenH);
                    // Draw pause overlay on top
                    pauseOverlay.render(screenW, screenH);
                    fflush(stdout);
                }
                break;
            }

            // ---------------------------------------------------------------
            // DEAD (offline) — red tint overlay with respawn timer
            // ---------------------------------------------------------------
            case GameState::Dead: {
                gameTime += dt;

                DeathResult dr = deathOverlay.update(inputState, dt);
                if (dr == DeathResult::Respawn) {
                    respawnPlayer();
                } else if (dr == DeathResult::ToMenu) {
                    autoSave(saveSystem, player, worldSeed, currentWorldName);
                    mainMenuScreen.setCanContinue(true);
                    mainMenuScreen.setSaves(saveSystem.listSaves());
                    isDead = false;
                    switchState(GameState::MainMenu, &mainMenuScreen);
                }

                if (state == GameState::Dead) {
                    camera.setFromPlayer(player.eyePosition(), player.yaw(), player.pitch());

                    std::vector<SceneObject> allObjects(world.objects().begin(),
                                                         world.objects().end());
                    worldState.gatherSceneObjects(allObjects, weaponMeshes, gameTime);

                    fb.clear();
                    renderer.render(allObjects, world.lights(), world.lightGrid(),
                                    camera, fb);

                    // Render beam/effect overlays
                    {
                        std::vector<SceneObject> overlayObjs;
                        worldState.gatherOverlayObjects(overlayObjs, weaponMeshes, gameTime);
                        if (!overlayObjs.empty()) {
                            renderer.render(overlayObjs, world.lights(), world.lightGrid(),
                                            camera, fb);
                        }
                    }

                    // Red death tint
                    fb.applyTint(Color3(1.6f, 0.25f, 0.2f), 0.55f);
                    fb.render();

                    renderHUD(screenW, screenH, player, gameTime, nullptr, 0.0f, 100.0f,
                              localKills, localDeaths);
                    killFeed.update(dt);
                    killFeed.render(screenW, screenH);
                    deathOverlay.render(screenW, screenH);
                    fflush(stdout);
                }
                break;
            }

            // ---------------------------------------------------------------
            // ONLINE DEAD — red tint overlay, still in multiplayer
            // ---------------------------------------------------------------
            case GameState::OnlineDead: {
                gameTime += dt;

                // Check if host disconnected (client only)
                if (lobby.isClient() && lobby.clientState() == ClientSession::State::Failed) {
                    lobby.disconnect();
                    isOnlineGame = false;
                    hasActiveGame = false;
                    isDead = false;
                    inputState.consumePresses();
                    modeSelectScreen.setError("Host disconnected");
                    switchState(GameState::ModeSelect, &modeSelectScreen);
                    break;
                }

                DeathResult dr = deathOverlay.update(inputState, dt);
                if (dr == DeathResult::Respawn) {
                    respawnPlayer();
                } else if (dr == DeathResult::ToMenu) {
                    lobby.disconnect();
                    isOnlineGame = false;
                    hasActiveGame = false;
                    isDead = false;
                    switchState(GameState::ModeSelect, &modeSelectScreen);
                }

                if (state == GameState::OnlineDead) {
                    // Keep sending position to peers
                    PlayerNetState localState;
                    localState.id = lobby.localPeerId();
                    localState.setPosition(player.position());
                    localState.yaw = player.yaw();
                    localState.pitch = player.pitch();
                    localState.flags = encodeWeaponFlags(
                        0, player.inventory().activeItem().type, false, true);  // isDead = true
                    lobby.update(dt, localState);

                    auto remotePlayers = lobby.getRemotePlayers();
                    for (auto* rp : remotePlayers) rp->update(dt);

                    // Keep world state ticking
                    std::vector<PlayerSnapshot> snaps;
                    snaps.push_back(PlayerSnapshot{lobby.localPeerId(), player.position(), player.radius(), player.height()});
                    for (auto* rp : remotePlayers) {
                        snaps.push_back(PlayerSnapshot{rp->peerId(), rp->position(), 0.3f, 1.8f});
                        ensureHealthEntry(rp->peerId());
                    }

                    std::vector<DamageEvent> damageEvents;
                    worldState.update(dt, gameTime, world.colliders(), lobby.isHosting(), snaps, damageEvents);
                    if (lobby.isHosting()) {
                        applyAuthoritativeDamage(damageEvents);
                    }

                    std::vector<SceneObject> allObjects(world.objects().begin(),
                                                         world.objects().end());
                    worldState.gatherSceneObjects(allObjects, weaponMeshes, gameTime);

                    for (auto* rp : remotePlayers) {
                        allObjects.push_back(rp->bodyObject());
                        allObjects.push_back(rp->headObject());
                        if (rp->activeWeapon() != ItemType::None) {
                            float rpAtk = rp->isAttacking() ? 0.5f : -1.0f;
                            auto wpnObjs = weaponMeshes.getThirdPersonObjects(
                                rp->activeWeapon(), rp->position(), rp->yaw(), rpAtk);
                            for (auto& o : wpnObjs) allObjects.push_back(std::move(o));
                        }
                    }

                    camera.setFromPlayer(player.eyePosition(), player.yaw(), player.pitch());
                    fb.clear();
                    renderer.render(allObjects, world.lights(), world.lightGrid(),
                                    camera, fb);

                    // Render beam/effect overlays
                    {
                        std::vector<SceneObject> overlayObjs;
                        worldState.gatherOverlayObjects(overlayObjs, weaponMeshes, gameTime);
                        if (!overlayObjs.empty()) {
                            renderer.render(overlayObjs, world.lights(), world.lightGrid(),
                                            camera, fb);
                        }
                    }

                    // Red death tint
                    fb.applyTint(Color3(1.6f, 0.25f, 0.2f), 0.55f);
                    fb.render();
                    renderRemoteNameLabels(camera, remotePlayers, screenW, screenH);

                    renderHUD(screenW, screenH, player, gameTime, nullptr, 0.0f, 100.0f,
                              localKills, localDeaths);

                    int playerCount = static_cast<int>(remotePlayers.size()) + 1;
                    char hudBuf[128];
                    snprintf(hudBuf, sizeof(hudBuf),
                             "\033[%d;1H\033[36mOnline: %d player%s | %s\033[0m",
                             screenH + 1, playerCount,
                             playerCount == 1 ? "" : "s",
                             lobby.isHosting() ? "Hosting" : "Connected");
                    printf("%s", hudBuf);
                    killFeed.update(dt);
                    killFeed.render(screenW, screenH);
                    deathOverlay.render(screenW, screenH);
                    fflush(stdout);
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
