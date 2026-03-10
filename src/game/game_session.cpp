#include "game/game_session.h"
#include "game/firebase_client.h"
#include <thread>
#include <iostream>
#include <csignal>
#include <sys/stat.h>
#include <cerrno>

extern void getTerminalSize(int& w, int& h);

static void clearScreen() {
    printf("\033[2J");
    fflush(stdout);
}

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

static bool mkdirRecursive(const std::string& path) {
    if (path.empty()) return false;
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return true;
    size_t pos = path.rfind('/');
    if (pos != std::string::npos && pos > 0) {
        mkdirRecursive(path.substr(0, pos));
    }
    return mkdir(path.c_str(), 0755) == 0 || (errno == EEXIST);
}

static void saveUsername(const std::string& path, const std::string& name) {
    std::string dir = path.substr(0, path.rfind('/'));
    mkdirRecursive(dir);
    FILE* f = fopen(path.c_str(), "w");
    if (f) {
        fprintf(f, "%s\n", name.c_str());
        fclose(f);
    }
}

static CharacterAppearance loadAppearance(const std::string& configDir) {
    std::string path = configDir + "/appearance";
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return CharacterAppearance();
    char buf[32];
    if (fgets(buf, sizeof(buf), f)) {
        fclose(f);
        int val = atoi(buf);
        return CharacterAppearance::deserialize(static_cast<uint8_t>(val));
    }
    fclose(f);
    return CharacterAppearance();
}

static void saveAppearance(const std::string& configDir, const CharacterAppearance& a) {
    mkdirRecursive(configDir);
    std::string path = configDir + "/appearance";
    FILE* f = fopen(path.c_str(), "w");
    if (f) {
        fprintf(f, "%d\n", static_cast<int>(a.serialize()));
        fclose(f);
    }
}

static int loadTargetFPS(const std::string& configDir) {
    std::string path = configDir + "/target_fps";
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return 30;  // default
    char buf[16];
    if (fgets(buf, sizeof(buf), f)) {
        fclose(f);
        int val = atoi(buf);
        // Clamp to valid range
        if (val < 20) val = 20;
        if (val > 100) val = 100;
        return val;
    }
    fclose(f);
    return 30;
}

static void saveTargetFPS(const std::string& configDir, int targetFPS) {
    mkdirRecursive(configDir);
    std::string path = configDir + "/target_fps";
    FILE* f = fopen(path.c_str(), "w");
    if (f) {
        fprintf(f, "%d\n", targetFPS);
        fclose(f);
    }
}

GameSession::GameSession(const std::string& configDir, const std::string& clientUUID,
                         const std::string& savedUsername, bool forceUsernameEntry)
    : m_configDir(configDir)
    , m_configPath(configDir + "/config")
    , m_clientUUID(clientUUID)
    , m_savedUsername(savedUsername)
    , m_localAppearance(loadAppearance(configDir))
    , m_fb(1, 1)  // Will be resized in run()
    , m_netManager(clientUUID, savedUsername)
{
    if (forceUsernameEntry) m_savedUsername.clear();

    m_input.m_sharedState = &m_inputState;
    m_input.startCapture();

    m_mainMenuScreen.setSaves(m_saveSystem.listSaves());
    m_loadWorldScreen.setSaves(m_saveSystem.listSaves());
    m_activeScreen = &m_mainMenuScreen;

    // Bind HostAuthority to game state - it needs WorldState for item drops
    // and World for spawn position queries. World is bound later after generation.
    m_worldState.bind(m_activeState.entities);
    m_netManager.hostAuthority().bind(m_activeState, m_worldState, nullptr);

    // Set up the local player sync callback so HostAuthority can update our
    // local Player object when host-side changes happen (death, respawn).
    m_netManager.hostAuthority().setLocalPlayerSync(m_clientUUID,
        [this](const PlayerState& ps) { syncFromPlayerState(ps); });

    // Load settings
    m_targetFPS = loadTargetFPS(m_configDir);
    m_lastSavedFPS = m_targetFPS;
    m_targetFrameTime = 1.0f / static_cast<float>(m_targetFPS);

    m_lastTime = Clock::now();
}

void GameSession::syncFromPlayerState(const PlayerState& ps) {
    // Called by HostAuthority when the local player's state changes
    // (death, respawn, pickup, drop). Syncs PlayerState -> Player object.

    // Always sync inventory
    m_player.inventory() = Inventory();
    for (int i = 0; i < 9; i++) {
        if (ps.inventory[i].type != ItemType::None)
            m_player.inventory().getSlot(i) = {ps.inventory[i].type, ps.inventory[i].count};
    }
    m_player.inventory().selectSlot(ps.selectedSlot);

    if (ps.isDead) {
        // Death: reset combat state
        m_combat = CombatSystem();
    } else if (m_state == GameState::Dead) {
        // Respawn: sync position and transition to playing
        m_player.setPosition(ps.position);
        m_player.setYaw(ps.yaw);
        m_player.setPitch(ps.pitch);
        m_combat = CombatSystem();
        m_input.setMouseCapture(true);
        m_inputState.consumePresses();
        m_state = GameState::Playing;
    }
    // For pickup/drop while alive: inventory already synced above, nothing else to do.
}

void GameSession::run() {
    getTerminalSize(m_screenW, m_screenH);
    m_screenH = m_screenH - 1;
    if (m_screenH < 10) m_screenH = 10;
    m_fb = Framebuffer(m_screenW, m_screenH);

    clearScreen();
    printf("\033[?25l");
    fflush(stdout);

    while (!m_inputState.quit) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - m_lastTime).count();
        m_lastTime = now;
        if (dt > 0.1f) dt = 0.1f;
        m_input.poll(m_inputState);

        int newW, newH;
        getTerminalSize(newW, newH);
        newH = newH - 1;
        if (newH < 10) newH = 10;
        if (newW != m_screenW || newH != m_screenH) {
            m_screenW = newW;
            m_screenH = newH;
            m_fb = Framebuffer(m_screenW, m_screenH);
            clearScreen();
        }

        if (m_inputState.focusLost.exchange(false, std::memory_order_relaxed)) {
            if (m_state == GameState::Playing) enterPause();
        }

        try {
            switch (m_state) {
                case GameState::MainMenu:    updateMainMenu(); break;
                case GameState::PlayMenu:    updatePlayMenu(); break;
                case GameState::LobbyBrowser: updateLobbyBrowser(dt); break;
                case GameState::LoadWorld:   updateLoadWorld(); break;
                case GameState::Connecting:  updateConnecting(dt); break;
                case GameState::Customize:   updateCustomize(); break;
                case GameState::Generating:  updateGenerating(); break;
                case GameState::Playing:     updatePlaying(dt); break;
                case GameState::Paused:      updatePaused(dt); break;
                case GameState::Dead:        updateDead(dt); break;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Main] Unhandled exception: " << e.what() << std::endl;
            break;
        } catch (...) {
            std::cerr << "[Main] Unknown exception caught!" << std::endl;
            break;
        }

        auto frameEnd = Clock::now();
        float elapsed = std::chrono::duration<float>(frameEnd - now).count();
        float sleepTime = m_targetFrameTime - elapsed;
        if (sleepTime > 0.0f)
            std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(sleepTime * 1000000)));
    }

    printf("\033[2J\033[H\033[?25hGoodbye!\n");
}

// --- State transitions ---

void GameSession::switchState(GameState newState, MenuScreen* screen) {
    m_state = newState;
    m_activeScreen = screen;
    clearScreen();
}

void GameSession::enterPause() {
    m_input.setMouseCapture(false);
    m_pauseOverlay.reset();
    m_pauseOverlay.startRename(m_activeState.metadata.name);
    m_pauseOverlay.stopRename();
    m_state = GameState::Paused;
}

void GameSession::resumePlay() {
    m_input.setMouseCapture(true);
    m_inputState.consumePresses();
    m_state = GameState::Playing;
}

void GameSession::enterDeath() {
    m_input.setMouseCapture(false);
    m_deathOverlay.activate(m_lastDamagerName);
    m_state = GameState::Dead;
}

// --- Per-state updates ---

void GameSession::updateMainMenu() {
    MenuResult result = m_activeScreen->update(m_inputState, m_screenW, m_screenH);
    if (result == MenuResult::Continue) {
        m_state = GameState::Playing;
        m_activeScreen = nullptr;
        m_input.setMouseCapture(true);
        clearScreen();
    } else if (result == MenuResult::Play) {
        switchState(GameState::PlayMenu, &m_playMenuScreen);
    } else if (result == MenuResult::CustomizeChar) {
        m_customizeScreen.setUsername(m_savedUsername);
        m_customizeScreen.setAppearance(m_localAppearance);
        m_customizeScreen.reset();
        switchState(GameState::Customize, &m_customizeScreen);
    } else if (result == MenuResult::Quit) {
        if (m_hasActiveGame) {
            doAutoSave();
        }
        m_inputState.quit = true;
    }
    if (m_state == GameState::MainMenu) m_activeScreen->render(m_screenW, m_screenH);
}

void GameSession::updatePlayMenu() {
    MenuResult result = m_activeScreen->update(m_inputState, m_screenW, m_screenH);
    if (result == MenuResult::OfflinePlay) {
        m_loadWorldScreen.setSaves(m_saveSystem.listSaves());
        switchState(GameState::LoadWorld, &m_loadWorldScreen);
    } else if (result == MenuResult::OnlinePlay) {
        m_netManager.refreshLobbies();
        m_lobbyBrowserScreen.setLobbies(m_netManager.discoveredLobbies());
        m_lobbyRefreshTimer = 0.0f;
        switchState(GameState::LobbyBrowser, &m_lobbyBrowserScreen);
    } else if (result == MenuResult::Back) {
        m_mainMenuScreen.setCanContinue(m_hasActiveGame);
        switchState(GameState::MainMenu, &m_mainMenuScreen);
    }
    if (m_state == GameState::PlayMenu) m_activeScreen->render(m_screenW, m_screenH);
}

void GameSession::updateLobbyBrowser(float dt) {
    m_lobbyRefreshTimer += dt;
    if (m_lobbyRefreshTimer >= 1.0f) {
        m_lobbyRefreshTimer = 0.0f;
        m_netManager.refreshLobbies();
        m_lobbyBrowserScreen.setLobbies(m_netManager.discoveredLobbies());
    }
    MenuResult result = m_activeScreen->update(m_inputState, m_screenW, m_screenH);
    if (result == MenuResult::JoinGame) {
        std::string uuid = m_lobbyBrowserScreen.getSelectedUUID();
        if (!uuid.empty()) {
            m_netManager.joinLobby(uuid);
            m_state = GameState::Connecting;
            m_isDiscoverable = false;  // We're a client now, not discoverable
            m_activeScreen = nullptr;
        }
    } else if (result == MenuResult::Refresh) {
        m_lobbyRefreshTimer = 0.0f;
        m_netManager.refreshLobbies();
        m_lobbyBrowserScreen.setLobbies(m_netManager.discoveredLobbies());
    } else if (result == MenuResult::Back) {
        switchState(GameState::PlayMenu, &m_playMenuScreen);
    }
    if (m_state == GameState::LobbyBrowser) m_activeScreen->render(m_screenW, m_screenH);
}

void GameSession::updateLoadWorld() {
    MenuResult result = m_activeScreen->update(m_inputState, m_screenW, m_screenH);
    if (result == MenuResult::NewGame) {
        m_activeState = RootState();
        m_activeState.world.seed = static_cast<unsigned int>(std::time(nullptr));
        m_activeState.metadata.name = "World " + std::to_string(m_activeState.world.seed % 10000);
        m_worldState.bind(m_activeState.entities);
        m_netManager.hostAuthority().bind(m_activeState, m_worldState, nullptr);
        m_generationStep = 0;
        m_state = GameState::Generating;
        m_activeScreen = nullptr;
        clearScreen();
    } else if (result == MenuResult::LoadWorld) {
        const auto& summary = m_loadWorldScreen.selectedSave();
        if (m_saveSystem.load(summary.filename, m_activeState)) {
            m_worldState.bind(m_activeState.entities);
            m_netManager.hostAuthority().bind(m_activeState, m_worldState, nullptr);
            m_generationStep = 0;
            m_state = GameState::Generating;
            m_activeScreen = nullptr;
            clearScreen();
        }
    } else if (result == MenuResult::Back) {
        switchState(GameState::PlayMenu, &m_playMenuScreen);
    }
    if (m_state == GameState::LoadWorld) m_activeScreen->render(m_screenW, m_screenH);
}

void GameSession::updateConnecting(float dt) {
    std::string out;
    MenuScreen::beginOutput(out, m_screenW, m_screenH);
    for (int i = 0; i < m_screenH / 2 - 2; ++i)
        MenuScreen::renderEmptyLine(out, m_screenW, MenuScreen::BG);
    MenuScreen::renderCenteredText(out, "ESTABLISHING CONNECTION...", m_screenW, MenuScreen::TITLE_FG, MenuScreen::BG);
    MenuScreen::renderCenteredText(out, "Please wait...", m_screenW, MenuScreen::SUBTITLE_FG, MenuScreen::BG);
    for (int i = m_screenH / 2 + 1; i < m_screenH; ++i)
        MenuScreen::renderEmptyLine(out, m_screenW, MenuScreen::BG);
    MenuScreen::flushOutput(out);

    m_netManager.update(dt, m_activeState);
    if (m_netManager.peerCount() > 0 && m_activeState.world.seed != 0) {
        m_worldState.bind(m_activeState.entities);
        m_generationStep = 0;
        m_state = GameState::Generating;
        clearScreen();
    }

    auto presses = m_inputState.consumePresses();
    for (auto k : presses) {
        if (k == KeyPress::Back) {
            m_netManager.disconnect();
            switchState(GameState::LobbyBrowser, &m_lobbyBrowserScreen);
            break;
        }
    }
}

void GameSession::updateCustomize() {
    MenuResult result = m_activeScreen->update(m_inputState, m_screenW, m_screenH);
    if (result == MenuResult::Back) {
        m_savedUsername = m_customizeScreen.username();
        m_localAppearance = m_customizeScreen.appearance();
        saveUsername(m_configPath, m_savedUsername);
        saveAppearance(m_configDir, m_localAppearance);
        m_netManager.setPlayerName(m_savedUsername);
        if (m_hasActiveGame && m_activeState.players.count(m_clientUUID)) {
            m_activeState.players[m_clientUUID].name = m_savedUsername;
            m_activeState.players[m_clientUUID].appearance = m_localAppearance;
        }
        switchState(GameState::MainMenu, &m_mainMenuScreen);
    }
    if (m_state == GameState::Customize) m_activeScreen->render(m_screenW, m_screenH);
}

void GameSession::updateGenerating() {
    int totalChunks = World::WORLD_CHUNKS * World::WORLD_CHUNKS;
    if (m_generationStep == 0) {
        GeneratingScreen::render(m_screenW, m_screenH, 0, totalChunks);
        m_generationStep = 1;
    } else if (m_generationStep == 1) {
        GeneratingScreen::render(m_screenW, m_screenH, totalChunks / 2, totalChunks);
        m_generationStep = 2;
    } else if (m_generationStep == 2) {
        m_world.generate(m_activeState.world.seed);
        m_player.setTerrainQuery(&m_world);

        // Re-bind HostAuthority now that World is generated (for spawn position queries)
        m_netManager.hostAuthority().bind(m_activeState, m_worldState, &m_world);

        auto it = m_activeState.players.find(m_clientUUID);
        float worldCenter = (World::WORLD_CHUNKS * World::CHUNK_SIZE) / 2.0f;
        float spawnY = m_world.terrainHeightAt(worldCenter, worldCenter) + 2.0f;

        if (it != m_activeState.players.end() && it->second.health > 0.0f && !it->second.isDead) {
            // Restore from save (alive player with valid health)
            auto& ps = it->second;
            m_player.setPosition(ps.position);
            m_player.setYaw(ps.yaw);
            m_player.setPitch(ps.pitch);
            m_player.inventory() = Inventory();
            for (int i = 0; i < 9; i++) {
                if (ps.inventory[i].type != ItemType::None)
                    m_player.inventory().getSlot(i) = {ps.inventory[i].type, ps.inventory[i].count};
            }
            m_player.inventory().selectSlot(ps.selectedSlot);
            ps.appearance = m_localAppearance;
            ps.name = m_savedUsername;
        } else if (it != m_activeState.players.end()) {
            // Returning player who was dead - respawn with empty inventory
            auto& ps = it->second;
            ps.position = {worldCenter, spawnY, worldCenter};
            ps.yaw = 0.0f;
            ps.pitch = 0.0f;
            ps.health = 100.0f;
            ps.isDead = false;
            ps.appearance = m_localAppearance;
            ps.name = m_savedUsername;
            m_player.setPosition(ps.position);
            m_player.setYaw(0.0f);
            m_player.setPitch(0.0f);
            m_player.inventory() = Inventory();
        } else {
            // Brand new player - give default loadout
            m_player.setPosition({worldCenter, spawnY, worldCenter});
            m_player.setYaw(0.0f);
            m_player.setPitch(0.0f);
            m_player.inventory() = Inventory();
            m_player.inventory().addItem(ItemType::Saber);
            m_player.inventory().addItem(ItemType::Laser);
            m_player.inventory().addItem(ItemType::Flashbang, 3);
            auto& ps = m_activeState.players[m_clientUUID];
            ps.uuid = m_clientUUID;
            ps.name = m_savedUsername;
            ps.health = 100.0f;
            ps.isDead = false;
            ps.appearance = m_localAppearance;
            ps.position = {worldCenter, spawnY, worldCenter};
            ps.yaw = 0.0f;
            ps.pitch = 0.0f;
            // Set PlayerState inventory to match (inventory is host-authoritative)
            ps.setDefaultLoadout();
        }

        m_hasActiveGame = true;
        m_combat = CombatSystem();
        m_timeSinceAutoSave = 0.0f;
        GeneratingScreen::render(m_screenW, m_screenH, totalChunks, totalChunks);
        m_generationStep = 3;
    } else {
        m_input.setMouseCapture(true);
        m_state = GameState::Playing;
        m_activeScreen = nullptr;
        clearScreen();
    }
}

void GameSession::updatePlaying(float dt) {
    // Networking update (handles packets, host authority processing, etc.)
    m_netManager.update(dt, m_activeState);

    // Check for host migration (client promoted to host)
    if (m_netManager.wasPromotedToHost()) {
        m_isDiscoverable = true;
        m_activeState.world.hostUUID = m_clientUUID;
        // Re-bind HostAuthority for the newly promoted host
        m_netManager.hostAuthority().bind(m_activeState, m_worldState, &m_world);
        m_netManager.hostAuthority().setLocalPlayerSync(m_clientUUID,
            [this](const PlayerState& ps) { syncFromPlayerState(ps); });
    }

    // Build set of active (connected) player UUIDs
    auto activePeers = m_netManager.activePeerUUIDs();

    // Sync local player state
    {
        ItemType combatItem = m_combat.isAttacking() ? m_combat.attackingWith() : m_player.inventory().activeItem().type;
        float combatProg = m_combat.isAttacking() ? m_combat.attackProgress() : -1.0f;
        StateSync::syncLocalPlayer(m_activeState, m_clientUUID, m_player, m_savedUsername,
                                   m_localAppearance, combatItem, combatProg);
    }

    auto& ps = m_activeState.players[m_clientUUID];

    // Sync Player object inventory FROM PlayerState (host is authoritative for inventory)
    {
        Inventory& inv = m_player.inventory();
        bool differs = false;
        for (int i = 0; i < 9 && !differs; i++) {
            const auto& slot = inv.getSlot(i);
            if (slot.type != ps.inventory[i].type || slot.count != ps.inventory[i].count)
                differs = true;
        }
        if (differs) {
            inv = Inventory();
            for (int i = 0; i < 9; i++) {
                if (ps.inventory[i].type != ItemType::None)
                    inv.getSlot(i) = {ps.inventory[i].type, ps.inventory[i].count};
            }
            inv.selectSlot(ps.selectedSlot);
        }
    }

    m_activeState.world.gameTime += dt;

    // Periodic autosave (all players save so they can resume or take over as host)
    m_timeSinceAutoSave += dt;
    if (m_timeSinceAutoSave >= AUTO_SAVE_INTERVAL) {
        m_timeSinceAutoSave = 0.0f;
        doAutoSave();
    }

    // Process input
    auto presses = m_inputState.consumePresses();
    bool attackPressed = false;
    for (auto k : presses) {
        if (k == KeyPress::Back) { enterPause(); break; }
        else if (k == KeyPress::KeyQ) m_player.inventory().cycleLeft();
        else if (k == KeyPress::KeyE) m_player.inventory().cycleRight();
        else if (k >= KeyPress::Key1 && k <= KeyPress::Key9)
            m_player.inventory().selectSlot(static_cast<int>(k) - static_cast<int>(KeyPress::Key1));
        else if (k == KeyPress::KeyF) {
            int slot = m_player.inventory().selectedSlot();
            const auto& active = m_player.inventory().activeItem();
            if (!active.empty()) {
                float cy = std::cos(m_player.yaw()), sy = std::sin(m_player.yaw());
                Vec3 dropPos = m_player.position() + Vec3(-sy, 0.5f, -cy);
                m_netManager.dropItem(m_clientUUID, slot, dropPos);
            }
        }
        else if (k == KeyPress::KeyG) {
            auto* nearby = m_worldState.findNearestPickable(m_player.position());
            if (nearby) m_netManager.pickupItem(m_clientUUID, nearby->id);
        }
        else if (k == KeyPress::MouseLeft) attackPressed = true;
    }

    if (m_state != GameState::Playing) return;

    // Physics and combat
    m_player.update(m_inputState, dt, m_world.colliders(), m_world.slopes());
    auto spawned = m_combat.update(m_player, attackPressed, dt, 0);
    for (auto& e : spawned) {
        e.spawnTime = m_activeState.world.gameTime;
        m_worldState.spawnEntity(e);
        // If a consumable was used (e.g., flashbang thrown), tell the host to decrement inventory
        if (e.type == EntityType::Projectile) {
            if (auto* pd = std::get_if<ProjectileData>(&e.data)) {
                if (pd->sourceType == ItemType::Flashbang) {
                    m_netManager.useItem(m_clientUUID, m_player.inventory().selectedSlot());
                }
            }
        }
    }

    // Build snapshots for damage detection
    std::vector<PlayerSnapshot> snaps;
    snaps.push_back({0, m_player.position(), m_player.radius(), m_player.height()});
    {
        uint8_t pid = 1;
        for (const auto& [uid, rps] : m_activeState.players) {
            if (uid == m_clientUUID) continue;
            if (activePeers.find(uid) == activePeers.end()) continue;
            if (rps.isDead) continue;
            if (rps.position.x == 0.0f && rps.position.y == 0.0f && rps.position.z == 0.0f) continue;
            snaps.push_back({pid, rps.position, 0.3f, 1.8f});
            pid++;
            if (pid >= 8) break;
        }
    }

    // World physics and damage detection
    std::vector<DamageEvent> damageEvents;
    m_worldState.update(dt, m_activeState.world.gameTime, m_world.colliders(), true, snaps, damageEvents);

    // Process damage events - unified path, no host/client branching!
    for (const auto& ev : damageEvents) {
        if (ev.targetId == 0) {
            // Local player hit - report to host (loopback if we're host)
            m_netManager.reportDamage(m_clientUUID, ev.amount, m_clientUUID);
        } else {
            // Remote player hit - find their UUID
            uint8_t pid = 1;
            for (auto& [uid, rps] : m_activeState.players) {
                if (uid == m_clientUUID) continue;
                if (activePeers.find(uid) == activePeers.end()) continue;
                if (rps.isDead) continue;
                if (rps.position.x == 0.0f && rps.position.y == 0.0f && rps.position.z == 0.0f) continue;
                if (pid == ev.targetId) {
                    m_netManager.reportDamage(uid, ev.amount, m_clientUUID);
                    break;
                }
                pid++;
                if (pid >= 8) break;
            }
        }
    }

    // Death detection: isDead is always set authoritatively by the host.
    if (ps.isDead && m_state != GameState::Dead) {
        enterDeath();
        return;
    }

    // Render
    renderScene(activePeers);
    renderHeldWeapon();
    m_fb.render();
    StateSync::renderRemoteNameplates(m_activeState.players, m_clientUUID, activePeers,
                                       m_camera, m_screenW, m_screenH);
    auto* nearbyItem = m_worldState.findNearestPickable(m_player.position());
    renderHUD(m_screenW, m_screenH, m_player, m_activeState.world.gameTime, nearbyItem,
              ps.health, 100.0f, ps.kills, ps.deaths, m_netManager.peerCount(),
              m_netManager.mode() == NetworkingManager::Mode::Host && m_netManager.isOnline(),
              m_activeState.metadata.name.c_str());
    m_killFeed.update(dt);
    m_killFeed.render(m_screenW, m_screenH);
    fflush(stdout);
}

void GameSession::updatePaused(float dt) {
    // Keep networking alive while paused
    m_netManager.update(dt, m_activeState);

    if (m_netManager.wasPromotedToHost()) {
        m_isDiscoverable = true;
        m_activeState.world.hostUUID = m_clientUUID;
        m_netManager.hostAuthority().bind(m_activeState, m_worldState, &m_world);
        m_netManager.hostAuthority().setLocalPlayerSync(m_clientUUID,
            [this](const PlayerState& ps) { syncFromPlayerState(ps); });
    }

    auto activePeersPaused = m_netManager.activePeerUUIDs();
    auto& ps = m_activeState.players[m_clientUUID];

    // Keep game time advancing (world is still alive - items spin, other players move)
    m_activeState.world.gameTime += dt;

    // Death check
    if (ps.isDead && m_state != GameState::Dead) {
        enterDeath();
        return;
    }

    bool isOnline = m_netManager.isOnline();
    bool isHost = (m_netManager.mode() == NetworkingManager::Mode::Host);
    // Allow rename in singleplayer (offline) or when host online
    bool canRename = !isOnline || (isHost && isOnline);
    PauseResult pr = m_pauseOverlay.update(m_inputState, isOnline, canRename, m_targetFPS);
    
    // Update frame time if FPS changed and save to config
    if (m_targetFPS != m_lastSavedFPS) {
        m_targetFrameTime = 1.0f / static_cast<float>(m_targetFPS);
        saveTargetFPS(m_configDir, m_targetFPS);
        m_lastSavedFPS = m_targetFPS;
    }

    if (pr == PauseResult::Resume) {
        if (m_inputState.focused.load(std::memory_order_relaxed)) resumePlay();
    } else if (pr == PauseResult::RenameWorld) {
        m_activeState.metadata.name = m_pauseOverlay.renameBuffer();
        // Only update public lobby name if we're online and hosting
        if (isOnline && isHost) {
            m_netManager.makePublic(m_activeState.metadata.name, m_activeState);
        }
    } else if (pr == PauseResult::GoMultiplayer) {
        m_netManager.makePublic(m_activeState.metadata.name, m_activeState);
        m_isDiscoverable = true;
        resumePlay();
    } else if (pr == PauseResult::Disconnect) {
        m_netManager.disconnect();
        m_isDiscoverable = false;
        // Remove remote players from state
        for (auto it = m_activeState.players.begin(); it != m_activeState.players.end(); ) {
            if (it->first != m_clientUUID) it = m_activeState.players.erase(it);
            else ++it;
        }
        resumePlay();
    } else if (pr == PauseResult::ToMenu) {
        if (isOnline) {
            m_netManager.disconnect();
            m_isDiscoverable = false;
        }
        doAutoSave();
        m_mainMenuScreen.setCanContinue(true);
        m_mainMenuScreen.setSaves(m_saveSystem.listSaves());
        switchState(GameState::MainMenu, &m_mainMenuScreen);
    }

    if (m_state == GameState::Paused) {
        renderScene(activePeersPaused);
        m_fb.render();
        StateSync::renderRemoteNameplates(m_activeState.players, m_clientUUID, activePeersPaused,
                                           m_camera, m_screenW, m_screenH);
        auto* nearbyPaused = m_worldState.findNearestPickable(m_player.position());
        renderHUD(m_screenW, m_screenH, m_player, m_activeState.world.gameTime, nearbyPaused,
                  ps.health, 100.0f, ps.kills, ps.deaths, m_netManager.peerCount(),
                  m_netManager.mode() == NetworkingManager::Mode::Host && m_netManager.isOnline(),
                  m_activeState.metadata.name.c_str());
        m_killFeed.update(dt);
        m_killFeed.render(m_screenW, m_screenH);
        m_pauseOverlay.render(m_screenW, m_screenH, isOnline, canRename, m_targetFPS);
        fflush(stdout);
    }
}

void GameSession::updateDead(float dt) {
    // Keep networking alive while dead
    m_netManager.update(dt, m_activeState);

    if (m_netManager.wasPromotedToHost()) {
        m_isDiscoverable = true;
        m_activeState.world.hostUUID = m_clientUUID;
        m_netManager.hostAuthority().bind(m_activeState, m_worldState, &m_world);
        m_netManager.hostAuthority().setLocalPlayerSync(m_clientUUID,
            [this](const PlayerState& ps) { syncFromPlayerState(ps); });
    }

    auto activePeersDead = m_netManager.activePeerUUIDs();
    m_activeState.world.gameTime += dt;
    auto& ps = m_activeState.players[m_clientUUID];

    // Sync Player inventory from PlayerState (so death screen shows cleared inventory)
    {
        Inventory& inv = m_player.inventory();
        inv = Inventory();
        for (int i = 0; i < 9; i++) {
            if (ps.inventory[i].type != ItemType::None)
                inv.getSlot(i) = {ps.inventory[i].type, ps.inventory[i].count};
        }
        inv.selectSlot(ps.selectedSlot);
    }

    // Check if we've been respawned (host: via HostAuthority callback, client: via delta)
    // Either way, isDead goes false and we sync the Player object uniformly.
    if (!ps.isDead && m_state == GameState::Dead) {
        syncFromPlayerState(ps);
        return;
    }

    DeathResult dr = m_deathOverlay.update(m_inputState, dt);
    if (dr == DeathResult::Respawn) {
        // Unified: just request respawn. NetworkingManager routes appropriately.
        m_netManager.requestRespawn(m_clientUUID);
    } else if (dr == DeathResult::ToMenu) {
        if (m_netManager.isOnline()) {
            m_netManager.disconnect();
            m_isDiscoverable = false;
        }
        doAutoSave();
        m_mainMenuScreen.setCanContinue(true);
        m_mainMenuScreen.setSaves(m_saveSystem.listSaves());
        ps.isDead = false;
        switchState(GameState::MainMenu, &m_mainMenuScreen);
    }

    if (m_state == GameState::Dead) {
        renderScene(activePeersDead);
        m_fb.applyTint(Color3(1.6f, 0.25f, 0.2f), 0.55f);
        m_fb.render();
        renderHUD(m_screenW, m_screenH, m_player, m_activeState.world.gameTime, nullptr,
                  0.0f, 100.0f, ps.kills, ps.deaths, m_netManager.peerCount(),
                  m_netManager.mode() == NetworkingManager::Mode::Host && m_netManager.isOnline(),
                  m_activeState.metadata.name.c_str());
        m_killFeed.update(dt);
        m_killFeed.render(m_screenW, m_screenH);
        m_deathOverlay.render(m_screenW, m_screenH);
        fflush(stdout);
    }
}

// --- Rendering helpers ---

void GameSession::renderScene(const std::unordered_set<std::string>& activePeers) {
    m_camera.setFromPlayer(m_player.eyePosition(), m_player.yaw(), m_player.pitch());
    std::vector<SceneObject> allObjects(m_world.objects().begin(), m_world.objects().end());
    m_worldState.gatherSceneObjects(allObjects, m_weaponMeshes, m_activeState.world.gameTime);
    StateSync::gatherRemotePlayerObjects(allObjects, m_activeState.players, m_clientUUID,
                                          activePeers, m_weaponMeshes, m_activeState.world.gameTime);
    m_fb.clear();
    m_renderer.render(allObjects, m_world.lights(), m_world.lightGrid(), m_camera, m_fb);

    // Overlay pass (beams/effects)
    std::vector<SceneObject> overlayObjs;
    m_worldState.gatherOverlayObjects(overlayObjs, m_weaponMeshes, m_activeState.world.gameTime);
    if (!overlayObjs.empty())
        m_renderer.render(overlayObjs, m_world.lights(), m_world.lightGrid(), m_camera, m_fb);
}

void GameSession::renderHeldWeapon() {
    ItemType activeWeapon = m_player.inventory().activeItem().type;
    if (activeWeapon != ItemType::None) {
        float atkProgress = m_combat.isAttacking() ? m_combat.attackProgress() : -1.0f;
        auto heldObjs = m_weaponMeshes.getHeldObjects(activeWeapon, m_player.eyePosition(),
                                                       m_player.yaw(), m_player.pitch(), atkProgress);
        for (const auto& obj : heldObjs) {
            std::vector<SceneObject> heldVec = {obj};
            m_renderer.render(heldVec, m_world.lights(), m_world.lightGrid(), m_camera, m_fb);
        }
    }
}

// --- Autosave ---

void GameSession::doAutoSave() {
    m_activeState.metadata.timestamp = static_cast<int64_t>(std::time(nullptr));
    // Position/orientation are client-controlled, sync them before saving.
    // Inventory is already in PlayerState (host-authoritative).
    PlayerState& ps = m_activeState.players[m_clientUUID];
    ps.uuid = m_clientUUID;
    ps.position = m_player.position();
    ps.yaw = m_player.yaw();
    ps.pitch = m_player.pitch();
    ps.gameTime = m_activeState.world.gameTime;
    ps.selectedSlot = m_player.inventory().selectedSlot();
    m_saveSystem.save(m_activeState);
}
