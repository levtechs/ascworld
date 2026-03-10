#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <atomic>
#include <rtc/rtc.hpp>
#include <variant>
#include <thread>
#include <condition_variable>
#include <deque>
#include "game/root_state.h"
#include "game/host_authority.h"

enum class PacketType : uint8_t {
    FullState = 1,
    DeltaUpdate = 2,
    InputSnapshot = 3,
    Event = 4,
    Keepalive = 5,
    DamageReport = 6,
    RespawnRequest = 7,
    PickupRequest = 8,
    DropRequest = 9,
    UseItemRequest = 10
};

struct LobbyInfo {
    std::string uuid;
    std::string name;
    std::string hostName;
    int playerCount = 0;
    int64_t lastSeen = 0;
};

class NetworkingManager {
public:
    enum class Mode { Host, Client };

    NetworkingManager(const std::string& clientUUID, const std::string& playerName);
    ~NetworkingManager();

    void update(float dt, RootState& state);

    // --- Unified game actions (mode-agnostic) ---
    // The game loop calls these without caring whether we're host or client.
    // If Host: delegates to HostAuthority directly (loopback).
    // If Client: sends packet to remote host.

    // Report damage dealt to a player. The host will apply it authoritatively.
    void reportDamage(const std::string& victimUUID, float amount, const std::string& attackerUUID);

    // Request respawn for a player.
    void requestRespawn(const std::string& uuid);

    // Pick up a dropped item entity. Host removes entity and adds to player inventory.
    void pickupItem(const std::string& playerUUID, uint16_t entityId);

    // Drop an item from player's inventory slot at a world position.
    void dropItem(const std::string& playerUUID, int slot, const Vec3& dropPos);

    // Use/consume one item from a player's inventory slot (e.g., throwing a flashbang).
    void useItem(const std::string& playerUUID, int slot);

    // --- Host management ---
    // Make this game discoverable (start accepting connections).
    // Can be called at any time - the player is always the host of their own game.
    void makePublic(const std::string& lobbyName, RootState& state);
    void makePrivate();
    void setPlayerName(const std::string& name) { m_playerName = name; }

    // --- Client actions ---
    void joinLobby(const std::string& lobbyUUID);
    void disconnect();

    // --- Queries ---
    Mode mode() const { return m_mode; }
    bool isPublic() const { return m_isPublic; }
    bool isOnline() const { return m_isPublic || m_mode == Mode::Client; }
    const std::vector<LobbyInfo>& discoveredLobbies() const { return m_discoveredLobbies; }
    void refreshLobbies();

    // --- Host authority access ---
    HostAuthority& hostAuthority() { return m_hostAuthority; }

    // Statistics
    int peerCount() const;
    int getPing(const std::string& uuid) const;

    // Get set of currently connected peer UUIDs (does NOT include self)
    std::unordered_set<std::string> activePeerUUIDs() const;

    // Host migration: returns true once when client was promoted to host
    bool wasPromotedToHost();

    // Get lobby UUID (for host migration)
    const std::string& lobbyUUID() const { return m_lobbyUUID; }

private:
    // Internal send methods (only used when actually networked)
    void sendClientState(const RootState& state);
    void sendDamageReport(const std::string& victimUUID, float amount, const std::string& attackerUUID);
    void sendRespawnRequest();
    void sendPickupRequest(uint16_t entityId);
    void sendDropRequest(int slot, const Vec3& dropPos);
    void sendUseItemRequest(int slot);

    struct Peer {
        std::string uuid;
        std::shared_ptr<rtc::PeerConnection> pc;
        std::shared_ptr<rtc::DataChannel> dc;
        int64_t joinedAt = 0;
        int lastPing = 0;
        bool ready = false;
        bool fullStateSent = false;
        bool descriptionSent = false;
        bool remoteDescriptionSet = false;
        // Trickle ICE: outbound candidates queued by the onLocalCandidate callback
        std::deque<std::string> pendingLocalCandidates;
        std::mutex candidateMutex;
        int remoteCandidateIndex = 0; // tracks how many remote candidates we've consumed
        int localCandidateIndex = 0;  // tracks how many local candidates we've pushed to Firebase
        float timeSinceLastRecv = 0.0f;  // seconds since last packet/keepalive received
    };

    struct InboundPacket {
        std::string peerUUID;
        PacketType type;
        nlohmann::json payload;
    };

    void setupPeer(const std::string& peerUUID, bool isOffer);
    void runSignalingLoop();
    void handleSignaling();
    void handlePacket(const std::string& peerUUID, rtc::message_variant data);
    void broadcastDelta(const RootState& state);
    void sendFullState(Peer& peer, const RootState& state);

    std::string m_clientUUID;
    std::string m_playerName;
    std::string m_lobbyName;
    std::string m_lobbyUUID;
    std::string m_originalLobbyUUID; // preserved for cleanup after host migration

    std::atomic<Mode> m_mode{Mode::Host};  // Default: host of own game
    std::atomic<bool> m_isPublic{false};    // Not discoverable by default

    HostAuthority m_hostAuthority;

    std::unordered_map<std::string, std::unique_ptr<Peer>> m_peers;
    std::mutex m_peersMutex;

    std::vector<InboundPacket> m_packetQueue;
    std::mutex m_packetMutex;

    std::vector<std::string> m_respawnQueue;  // UUIDs of players requesting respawn (host-side)
    std::vector<std::string> m_deathQueue;    // UUIDs of players who just died via DamageReport (host-side)

    std::vector<LobbyInfo> m_discoveredLobbies;

    rtc::Configuration m_rtcConfig;
    std::atomic<bool> m_promotedToHost{false};
    std::atomic<bool> m_heartbeatNeeded{false}; // Set when peer count changes to trigger immediate Firebase update
    float m_keepaliveTimer = 0.0f;

    // Background thread for Firebase signaling
    std::thread m_signalingThread;
    std::atomic<bool> m_running{true};
    std::mutex m_sigMutex;
    std::condition_variable m_sigCV;
};
