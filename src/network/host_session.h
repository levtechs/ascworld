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

class HostSession {
public:
    HostSession(FirebaseClient& firebase);
    ~HostSession();

    // Create a new room. Returns the room ID.
    std::string createRoom(const std::string& roomName, const std::string& hostName,
                           uint32_t seed, bool isPublic, const std::string& password = "");

    // Start accepting connections (call after createRoom)
    void startAccepting();

    // Update: process messages, broadcast state. Call each frame.
    void update(float dt, const PlayerNetState& localState);

    // Get remote players for rendering
    std::vector<RemotePlayer*> getRemotePlayers();

    // Close the session, clean up Firebase
    void close();

    // Get room info
    const std::string& roomId() const { return m_roomId; }
    uint32_t seed() const { return m_seed; }
    int playerCount() const;

    // Set host's character appearance (call before createRoom or anytime)
    void setHostAppearance(uint8_t appearance) { m_hostAppearance = appearance; }
    uint8_t hostAppearance() const { return m_hostAppearance; }

    // Check if session is active
    bool isActive() const { return m_active; }

    // Callback for world-state events received from peers
    using WorldEventCallback = std::function<void(const uint8_t* data, size_t len)>;
    void setWorldEventCallback(WorldEventCallback cb) { m_onWorldEvent = std::move(cb); }

private:
    struct DisconnectInfo {
        std::string peerId;
        PeerId id = PEER_ID_INVALID;
        std::shared_ptr<PeerChannel> channel;
    };

    FirebaseClient& m_firebase;
    Signaling m_signaling;

    std::string m_roomId;
    std::string m_hostName;
    uint32_t m_seed = 0;
    uint8_t m_hostAppearance = 0;
    bool m_active = false;

    // Connected peers
    struct PeerInfo {
        std::string name;
        PeerId id = PEER_ID_INVALID;
        uint8_t appearance = 0; // packed CharacterAppearance byte
        std::shared_ptr<PeerChannel> channel;
        std::unique_ptr<RemotePlayer> player;
    };

    std::mutex m_mutex;
    std::unordered_map<std::string, PeerInfo> m_peers; // remotePeerId -> info
    PeerId m_nextPeerId = 1; // 0 is host

    // Timing
    float m_sendTimer = 0;
    float m_roomHeartbeatTimer = 0;

    // World event callback
    WorldEventCallback m_onWorldEvent;

    // Handle incoming connection
    void onNewPeer(const std::string& remotePeerId, const std::string& sdpOffer);

    // Handle messages from a peer
    void onPeerMessage(const std::string& remotePeerId, const uint8_t* data, size_t len, bool reliable);

public:
    // Broadcast a message to all connected peers (public for Lobby to forward item events)
    void broadcastReliable(const std::vector<uint8_t>& data, const std::string& excludePeerId = "");
    void broadcastUnreliable(const std::vector<uint8_t>& data, const std::string& excludePeerId = "");

private:

    // Remove a disconnected peer
    void removePeer(const std::string& remotePeerId);

    // Remove peer internals; caller closes channel outside lock
    bool collectPeerForRemoval(const std::string& remotePeerId, DisconnectInfo& out);

    // Finalize removal outside lock
    void finalizePeerRemoval(DisconnectInfo info);

    // Generate a unique room ID
    static std::string generateRoomId();
};
