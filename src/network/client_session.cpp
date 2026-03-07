#include "network/client_session.h"
#include <algorithm>
#include <random>
#include <ctime>

ClientSession::ClientSession(FirebaseClient& firebase)
    : m_firebase(firebase)
    , m_signaling(firebase)
{
}

ClientSession::~ClientSession() {
    leave();
}

static std::string generateLocalPeerId() {
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static constexpr int idLen = 6;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, sizeof(chars) - 2);

    std::string id;
    id.reserve(idLen);
    for (int i = 0; i < idLen; i++) {
        id += chars[dist(gen)];
    }
    return id;
}

std::vector<RoomInfo> ClientSession::listRooms() {
    std::vector<RoomInfo> rooms;

    json data = m_firebase.get("rooms");
    if (data.is_null() || !data.is_object()) return rooms;

    int64_t now = static_cast<int64_t>(std::time(nullptr));
    constexpr int64_t ACTIVE_THRESHOLD = 30;   // show only rooms with fresh heartbeat
    constexpr int64_t DELETE_THRESHOLD = 120;  // hard-delete stale rooms

    // Collect stale room IDs for cleanup
    std::vector<std::string> staleRoomIds;

    for (auto& [roomId, roomData] : data.items()) {
        if (!roomData.is_object()) continue;

        int64_t createdAt = roomData.value("createdAt", static_cast<int64_t>(0));
        int64_t age = (createdAt > 0) ? (now - createdAt) : DELETE_THRESHOLD + 1;

        // Hard stale: delete from Firebase.
        if (age > DELETE_THRESHOLD) {
            staleRoomIds.push_back(roomId);
            continue;
        }

        RoomInfo info;
        info.roomId = roomId;
        info.name = roomData.value("name", "");
        info.hostName = roomData.value("hostName", "");
        info.seed = roomData.value("seed", 0u);
        info.playerCount = roomData.value("playerCount", 0);
        info.maxPlayers = roomData.value("maxPlayers", MAX_PLAYERS);
        info.isPublic = roomData.value("isPublic", true);
        info.hasPassword = roomData.value("hasPassword", false);

        // Visibility filter:
        // - public
        // - has space
        // - heartbeat is recent (host is likely alive)
        if (info.isPublic && info.playerCount < info.maxPlayers && age <= ACTIVE_THRESHOLD) {
            rooms.push_back(info);
        }
    }

    // Clean up stale rooms from Firebase (best-effort)
    for (const auto& staleId : staleRoomIds) {
        m_firebase.del("rooms/" + staleId);
        m_firebase.del("signaling/" + staleId);
    }

    std::sort(rooms.begin(), rooms.end(), [](const RoomInfo& a, const RoomInfo& b) {
        if (a.name == b.name) return a.roomId < b.roomId;
        return a.name < b.name;
    });

    return rooms;
}

bool ClientSession::joinRoom(const std::string& roomId, const std::string& playerName,
                              const std::string& password) {
    // Verify room exists
    json roomData = m_firebase.get("rooms/" + roomId);
    if (roomData.is_null()) return false;

    m_roomId = roomId;
    m_roomName = roomData.value("name", "Unnamed Room");
    m_playerName = playerName;

    // Generate a local peer ID string for signaling
    std::string localPeerIdStr = generateLocalPeerId();
    // Set up signaling
    m_signaling.setRoom(roomId, localPeerIdStr);

    // Create PeerChannel as offerer
    m_hostChannel = std::make_unique<PeerChannel>();
    m_hostChannel->create(true); // offerer

    // Set up signaling callbacks
    m_hostChannel->onLocalDescription([this](const std::string& sdp) {
        m_signaling.sendOffer("host", sdp);
    });

    m_hostChannel->onLocalCandidate([this](const std::string& candidate, const std::string& mid) {
        m_signaling.sendCandidate("host", candidate, mid, 0);
    });

    // Listen for answer from host
    m_signaling.listenForAnswer("host", [this](const std::string& sdpAnswer) {
        if (m_hostChannel) {
            m_hostChannel->setRemoteDescription(sdpAnswer, "answer");
        }
    });

    // Listen for remote ICE candidates from host
    m_signaling.listenForCandidates("host",
        [this](const std::string& candidate, const std::string& mid, int /*sdpMLineIndex*/) {
            if (m_hostChannel) {
                m_hostChannel->addRemoteCandidate(candidate, mid);
            }
        });

    // Connection state handler
    m_hostChannel->onConnected([this](bool connected) {
        if (connected) {
            setState(State::Connected);

            // Send our name to the host
            PlayerJoinMsg joinMsg;
            joinMsg.id = PEER_ID_INVALID; // host will assign real ID
            joinMsg.name = m_playerName;
            if (m_hostChannel) {
                m_hostChannel->sendReliable(joinMsg.serialize());
            }
        } else {
            setState(State::Failed);
        }
    });

    // Message handlers
    m_hostChannel->onReliableMessage([this](const uint8_t* data, size_t len) {
        onHostMessage(data, len, true);
    });

    m_hostChannel->onUnreliableMessage([this](const uint8_t* data, size_t len) {
        onHostMessage(data, len, false);
    });

    setState(State::Connecting);
    return true;
}

void ClientSession::update(float dt, const PlayerNetState& localState) {
    if (m_state != State::Connected) return;

    m_sendTimer += dt;
    if (m_sendTimer >= NET_SEND_INTERVAL) {
        m_sendTimer = 0;

        // Send local state over unreliable channel
        if (m_hostChannel && m_hostChannel->isConnected()) {
            m_hostChannel->sendUnreliable(localState.serialize());
        }
    }

    // Update all remote players
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [id, player] : m_remotePlayers) {
        player->update(dt);
    }
}

void ClientSession::onHostMessage(const uint8_t* data, size_t len, bool /*reliable*/) {
    if (len == 0) return;

    NetMsgType type = getMessageType(data, len);

    switch (type) {
        case NetMsgType::AssignId: {
            AssignIdMsg msg;
            if (AssignIdMsg::deserialize(data, len, msg)) {
                m_localPeerId = msg.id;
            }
            break;
        }
        case NetMsgType::WorldSeed: {
            WorldSeedMsg msg;
            if (WorldSeedMsg::deserialize(data, len, msg)) {
                m_worldSeed = msg.seed;
                m_hasWorldSeed = true;
            }
            break;
        }
        case NetMsgType::PlayerState: {
            PlayerNetState state;
            if (!PlayerNetState::deserialize(data, len, state)) return;

            // Skip our own state
            if (state.id == m_localPeerId) return;

            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_remotePlayers.find(state.id);
            if (it != m_remotePlayers.end()) {
                it->second->onNetworkState(state);
            } else {
                // Create remote player on first state update if we haven't seen a join msg
                auto player = std::make_unique<RemotePlayer>(state.id, "");
                player->onNetworkState(state);
                m_remotePlayers[state.id] = std::move(player);
            }
            break;
        }
        case NetMsgType::PlayerJoin: {
            PlayerJoinMsg msg;
            if (!PlayerJoinMsg::deserialize(data, len, msg)) return;

            // Don't create a remote player for ourselves
            if (msg.id == m_localPeerId) return;

            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_remotePlayers.find(msg.id);
            if (it == m_remotePlayers.end()) {
                m_remotePlayers[msg.id] = std::make_unique<RemotePlayer>(msg.id, msg.name);
            } else {
                it->second->setName(msg.name);
            }
            break;
        }
        case NetMsgType::PlayerLeave: {
            PlayerLeaveMsg msg;
            if (!PlayerLeaveMsg::deserialize(data, len, msg)) return;

            std::lock_guard<std::mutex> lock(m_mutex);
            m_remotePlayers.erase(msg.id);
            break;
        }
        default:
            break;
    }
}

std::vector<RemotePlayer*> ClientSession::getRemotePlayers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RemotePlayer*> result;
    for (auto& [id, player] : m_remotePlayers) {
        result.push_back(player.get());
    }
    return result;
}

void ClientSession::leave() {
    if (m_state == State::Disconnected) return;

    // Send PlayerLeave to host before closing
    if (m_hostChannel && m_hostChannel->isConnected()) {
        try {
            PlayerLeaveMsg leaveMsg;
            leaveMsg.id = m_localPeerId;
            m_hostChannel->sendReliable(leaveMsg.serialize());
        } catch (...) {
        }
    }

    if (m_hostChannel) {
        m_hostChannel->close();
        m_hostChannel.reset();
    }

    m_signaling.stop();
    m_signaling.cleanup();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_remotePlayers.clear();
    }

    m_hasWorldSeed = false;
    m_localPeerId = PEER_ID_INVALID;
    setState(State::Disconnected);
}

void ClientSession::setState(State s) {
    m_state = s;
    if (m_stateCallback) {
        m_stateCallback(s);
    }
}
