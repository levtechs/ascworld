#pragma once
#include "network/firebase_client.h"
#include "network/signaling.h"
#include "network/peer_connection.h"
#include "network/remote_player.h"
#include "network/net_common.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <memory>
#include <functional>

class ClientSession {
public:
    ClientSession(FirebaseClient& firebase);
    ~ClientSession();

    // Browse available rooms
    std::vector<RoomInfo> listRooms();

    // Join a room by ID. Returns true if connection initiated.
    bool joinRoom(const std::string& roomId, const std::string& playerName,
                  const std::string& password = "");

    // Update: process messages, send local state. Call each frame.
    void update(float dt, const PlayerNetState& localState);

    // Get remote players for rendering (includes host)
    std::vector<RemotePlayer*> getRemotePlayers();

    // Leave the session
    void leave();

    // Get assigned peer ID
    PeerId localPeerId() const { return m_localPeerId; }

    // Room metadata
    const std::string& roomId() const { return m_roomId; }
    const std::string& roomName() const { return m_roomName; }
    const std::string& playerName() const { return m_playerName; }

    // Get the world seed (received from host)
    uint32_t worldSeed() const { return m_worldSeed; }
    bool hasWorldSeed() const { return m_hasWorldSeed; }

    // Connection state
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Failed
    };
    State state() const { return m_state; }

    // Callback when connection state changes
    using StateCallback = std::function<void(State newState)>;
    void onStateChange(StateCallback cb) { m_stateCallback = cb; }

private:
    FirebaseClient& m_firebase;
    Signaling m_signaling;

    std::string m_roomId;
    std::string m_roomName;
    std::string m_playerName;
    PeerId m_localPeerId = PEER_ID_INVALID;
    uint32_t m_worldSeed = 0;
    bool m_hasWorldSeed = false;

    State m_state = State::Disconnected;
    StateCallback m_stateCallback;

    // Connection to host
    std::unique_ptr<PeerChannel> m_hostChannel;

    // Remote players (host + other clients, relayed by host)
    std::mutex m_mutex;
    std::unordered_map<PeerId, std::unique_ptr<RemotePlayer>> m_remotePlayers;

    // Timing
    float m_sendTimer = 0;

    // Handle messages from host
    void onHostMessage(const uint8_t* data, size_t len, bool reliable);

    void setState(State s);
};
