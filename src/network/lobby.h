#pragma once
#include "network/firebase_client.h"
#include "network/host_session.h"
#include "network/client_session.h"
#include "network/net_common.h"
#include <string>
#include <vector>
#include <memory>

class Lobby {
public:
    Lobby();
    ~Lobby();

    // Initialize networking. Returns true on success.
    bool init();

    // Set the local player name
    void setPlayerName(const std::string& name) { m_playerName = name; }
    const std::string& playerName() const { return m_playerName; }

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

private:
    FirebaseClient m_firebase;
    std::string m_playerName = "Player";

    std::unique_ptr<HostSession> m_hostSession;
    std::unique_ptr<ClientSession> m_clientSession;
};
