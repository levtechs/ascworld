#pragma once
#include "network/firebase_client.h"
#include "network/host_session.h"
#include "network/client_session.h"
#include "network/net_common.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

class Lobby {
public:
    Lobby();
    ~Lobby();

    // Initialize networking. Returns true on success.
    bool init();

    // Set the local player name
    void setPlayerName(const std::string& name) { m_playerName = name; }
    const std::string& playerName() const { return m_playerName; }

    // Set character appearance (packed byte)
    void setAppearance(uint8_t appearance) { m_appearance = appearance; }
    uint8_t appearance() const { return m_appearance; }

    // Browse rooms
    std::vector<RoomInfo> refreshRooms();

    // Host a new game
    // Returns room ID on success, empty string on failure
    std::string hostGame(const std::string& roomName, uint32_t seed,
                         bool isPublic = true, const std::string& password = "");

    // Join an existing game
    bool joinGame(const std::string& roomId, const std::string& password = "");

    // Update networking (call every frame when online)
    void update(float dt, const PlayerNetState& localState);

    // Get remote players for rendering
    std::vector<RemotePlayer*> getRemotePlayers();

    // Leave current game / close session
    void disconnect();

    // State queries
    bool isHosting() const { return m_hostSession != nullptr; }
    bool isClient() const { return m_clientSession != nullptr; }
    bool isOnline() const { return isHosting() || isClient(); }

    uint32_t worldSeed() const;
    bool hasWorldSeed() const;
    PeerId localPeerId() const;

    // Room name (for rehosting)
    std::string roomName() const;

    // Get connection state (for UI)
    ClientSession::State clientState() const;

    // Send a reliable message to the network (world events)
    void sendReliable(const std::vector<uint8_t>& data);

    // Set callback for incoming world events
    using WorldEventCallback = std::function<void(const uint8_t* data, size_t len)>;
    void setWorldEventCallback(WorldEventCallback cb);

private:
    FirebaseClient m_firebase;
    std::string m_playerName = "Player";
    uint8_t m_appearance = 0;

    std::unique_ptr<HostSession> m_hostSession;
    std::unique_ptr<ClientSession> m_clientSession;
    WorldEventCallback m_worldEventCallback;
};
