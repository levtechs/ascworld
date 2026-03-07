#include "network/host_session.h"
#include <random>
#include <chrono>
#include <ctime>
#include <iostream>

HostSession::HostSession(FirebaseClient& firebase)
    : m_firebase(firebase)
    , m_signaling(firebase)
{
}

HostSession::~HostSession() {
    close();
}

std::string HostSession::generateRoomId() {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static constexpr int idLen = 8;

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

std::string HostSession::createRoom(const std::string& roomName, const std::string& hostName,
                                     uint32_t seed, bool isPublic, const std::string& password) {
    m_roomId = generateRoomId();
    m_hostName = hostName;
    m_seed = seed;

    json roomData;
    roomData["name"] = roomName;
    roomData["hostName"] = hostName;
    roomData["seed"] = seed;
    roomData["playerCount"] = 1;
    roomData["maxPlayers"] = MAX_PLAYERS;
    roomData["isPublic"] = isPublic;
    roomData["hasPassword"] = !password.empty();
    roomData["createdAt"] = static_cast<int64_t>(std::time(nullptr));

    std::cerr << "[HOST] Creating room " << m_roomId << " name='" << roomName << "' seed=" << seed << std::endl;
    m_firebase.put("rooms/" + m_roomId, roomData);

    m_signaling.setRoom(m_roomId, "host");
    m_active = true;
    std::cerr << "[HOST] Room created, signaling set up" << std::endl;
    return m_roomId;
}

void HostSession::startAccepting() {
    std::cerr << "[HOST] Starting to accept peers (listening for offers)" << std::endl;
    m_signaling.listenForOffers([this](const std::string& remotePeerId, const std::string& sdpOffer) {
        std::cerr << "[HOST] Received offer from peer: " << remotePeerId << " (sdp length=" << sdpOffer.size() << ")" << std::endl;
        onNewPeer(remotePeerId, sdpOffer);
    });
}

void HostSession::onNewPeer(const std::string& remotePeerId, const std::string& sdpOffer) {
    PeerId assignedId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (static_cast<int>(m_peers.size()) + 1 >= MAX_PLAYERS) return;
        assignedId = m_nextPeerId++;
    }

    std::cerr << "[HOST] Creating answerer PeerChannel for " << remotePeerId << " (assignedId=" << (int)assignedId << ")" << std::endl;
    auto channel = std::make_shared<PeerChannel>();
    channel->create(false);

    channel->onLocalDescription([this, remotePeerId](const std::string& sdp) {
        std::cerr << "[HOST] Sending SDP answer to " << remotePeerId << " (length=" << sdp.size() << ")" << std::endl;
        m_signaling.sendAnswer(remotePeerId, sdp);
    });

    channel->onLocalCandidate([this, remotePeerId](const std::string& candidate, const std::string& mid) {
        std::cerr << "[HOST] Sending ICE candidate to " << remotePeerId << " mid=" << mid << std::endl;
        m_signaling.sendCandidate(remotePeerId, candidate, mid, 0);
    });

    channel->onConnected([this, remotePeerId, assignedId](bool connected) {
        std::cerr << "[HOST] onConnected for " << remotePeerId << " connected=" << connected << std::endl;
        if (!connected) {
            removePeer(remotePeerId);
            return;
        }

        std::shared_ptr<PeerChannel> newPeerChannel;
        std::vector<std::vector<uint8_t>> bootstrapMsgs;
        std::vector<std::shared_ptr<PeerChannel>> otherChannels;
        std::vector<uint8_t> newJoinData;
        int count = 1;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_peers.find(remotePeerId);
            if (it == m_peers.end() || !it->second.channel) return;

            newPeerChannel = it->second.channel;

            AssignIdMsg assignMsg;
            assignMsg.id = assignedId;
            bootstrapMsgs.push_back(assignMsg.serialize());

            WorldSeedMsg seedMsg;
            seedMsg.seed = m_seed;
            bootstrapMsgs.push_back(seedMsg.serialize());

            PlayerJoinMsg hostJoin;
            hostJoin.id = PEER_ID_HOST;
            hostJoin.name = m_hostName;
            bootstrapMsgs.push_back(hostJoin.serialize());

            for (auto& [otherPeerId, otherPeer] : m_peers) {
                if (otherPeerId == remotePeerId) continue;
                if (!otherPeer.channel || !otherPeer.channel->isConnected()) continue;
                PlayerJoinMsg joinMsg;
                joinMsg.id = otherPeer.id;
                joinMsg.name = otherPeer.name;
                bootstrapMsgs.push_back(joinMsg.serialize());
            }

            auto& p = it->second;
            if (!p.player) {
                p.player = std::make_unique<RemotePlayer>(assignedId, p.name);
            } else {
                p.player->setName(p.name);
            }

            PlayerJoinMsg newJoin;
            newJoin.id = assignedId;
            newJoin.name = p.name;
            newJoinData = newJoin.serialize();

            for (auto& [otherPeerId, otherPeer] : m_peers) {
                if (otherPeerId == remotePeerId) continue;
                if (otherPeer.channel && otherPeer.channel->isConnected()) {
                    otherChannels.push_back(otherPeer.channel);
                }
            }

            count = static_cast<int>(m_peers.size()) + 1;
        }

        std::cerr << "[HOST] Sending AssignId(" << (int)assignedId << ") to " << remotePeerId << std::endl;
        std::cerr << "[HOST] Sending WorldSeed(" << m_seed << ") to " << remotePeerId << std::endl;
        if (newPeerChannel && newPeerChannel->isConnected()) {
            for (const auto& msg : bootstrapMsgs) {
                newPeerChannel->sendReliable(msg);
            }
        }

        for (auto& ch : otherChannels) {
            if (ch && ch->isConnected()) {
                ch->sendReliable(newJoinData);
            }
        }

        m_firebase.patch("rooms/" + m_roomId, json{{"playerCount", count}});
    });

    channel->onReliableMessage([this, remotePeerId](const uint8_t* data, size_t len) {
        onPeerMessage(remotePeerId, data, len, true);
    });

    channel->onUnreliableMessage([this, remotePeerId](const uint8_t* data, size_t len) {
        onPeerMessage(remotePeerId, data, len, false);
    });

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& peer = m_peers[remotePeerId];
        peer.id = assignedId;
        peer.channel = channel;
    }

    std::cerr << "[HOST] Calling setRemoteDescription(offer) for " << remotePeerId << " (outside lock)" << std::endl;
    channel->setRemoteDescription(sdpOffer, "offer");
    std::cerr << "[HOST] setRemoteDescription(offer) returned for " << remotePeerId << std::endl;

    m_signaling.listenForCandidates(remotePeerId,
        [this, remotePeerId](const std::string& candidate, const std::string& mid, int /*sdpMLineIndex*/) {
            std::shared_ptr<PeerChannel> peerChannel;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_peers.find(remotePeerId);
                if (it != m_peers.end()) {
                    peerChannel = it->second.channel;
                }
            }
            if (peerChannel) {
                peerChannel->addRemoteCandidate(candidate, mid);
            }
        });
}

void HostSession::onPeerMessage(const std::string& remotePeerId, const uint8_t* data, size_t len, bool reliable) {
    (void)reliable;
    if (len == 0) return;

    NetMsgType type = getMessageType(data, len);

    switch (type) {
        case NetMsgType::PlayerState: {
            PlayerNetState state;
            if (!PlayerNetState::deserialize(data, len, state)) return;

            std::vector<std::shared_ptr<PeerChannel>> targets;
            std::vector<uint8_t> rebroadcast;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_peers.find(remotePeerId);
                if (it == m_peers.end()) return;

                state.id = it->second.id;
                if (it->second.player) {
                    it->second.player->onNetworkState(state);
                }

                rebroadcast = state.serialize();
                for (auto& [otherPeerId, otherPeer] : m_peers) {
                    if (otherPeerId == remotePeerId) continue;
                    if (otherPeer.channel && otherPeer.channel->isConnected()) {
                        targets.push_back(otherPeer.channel);
                    }
                }
            }

            for (auto& ch : targets) {
                ch->sendUnreliable(rebroadcast);
            }
            break;
        }

        case NetMsgType::PlayerJoin: {
            PlayerJoinMsg msg;
            if (!PlayerJoinMsg::deserialize(data, len, msg)) return;

            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_peers.find(remotePeerId);
            if (it != m_peers.end()) {
                it->second.name = msg.name;
                if (it->second.player) {
                    it->second.player->setName(msg.name);
                }
            }
            break;
        }

        case NetMsgType::PlayerLeave: {
            removePeer(remotePeerId);
            break;
        }

        default:
            break;
    }
}

void HostSession::update(float dt, const PlayerNetState& localState) {
    if (!m_active) return;

    m_sendTimer += dt;
    m_roomHeartbeatTimer += dt;

    if (m_roomHeartbeatTimer >= 2.0f) {
        m_roomHeartbeatTimer = 0.0f;
        int count = 1;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            count = static_cast<int>(m_peers.size()) + 1;
        }
        m_firebase.patch("rooms/" + m_roomId, json{
            {"playerCount", count},
            {"createdAt", static_cast<int64_t>(std::time(nullptr))}
        });
    }

    if (m_sendTimer >= NET_SEND_INTERVAL) {
        m_sendTimer = 0;

        struct PeerSnapshot {
            std::string peerId;
            std::shared_ptr<PeerChannel> channel;
            PeerId id = PEER_ID_INVALID;
            RemotePlayer* player = nullptr;
        };

        std::vector<PeerSnapshot> peers;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            peers.reserve(m_peers.size());
            for (auto& [peerId, peer] : m_peers) {
                PeerSnapshot s;
                s.peerId = peerId;
                s.channel = peer.channel;
                s.id = peer.id;
                s.player = peer.player.get();
                peers.push_back(std::move(s));
            }
        }

        auto hostData = localState.serialize();
        for (auto& peer : peers) {
            if (peer.channel && peer.channel->isConnected()) {
                peer.channel->sendUnreliable(hostData);
            }
        }

        for (auto& src : peers) {
            if (!src.player) continue;
            PlayerNetState peerState;
            peerState.id = src.id;
            Vec3 pos = src.player->position();
            peerState.x = pos.x;
            peerState.y = pos.y;
            peerState.z = pos.z;
            peerState.yaw = src.player->yaw();
            peerState.pitch = src.player->pitch();

            auto peerData = peerState.serialize();
            for (auto& dst : peers) {
                if (dst.peerId == src.peerId) continue;
                if (dst.channel && dst.channel->isConnected()) {
                    dst.channel->sendUnreliable(peerData);
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [peerId, peer] : m_peers) {
            if (peer.player) {
                peer.player->update(dt);
            }
        }
    }

    std::vector<std::string> staleIds;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [peerId, peer] : m_peers) {
            if (peer.player && peer.player->isStale()) {
                staleIds.push_back(peerId);
            }
        }
    }

    for (const auto& id : staleIds) {
        removePeer(id);
    }
}

std::vector<RemotePlayer*> HostSession::getRemotePlayers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RemotePlayer*> result;
    for (auto& [peerId, peer] : m_peers) {
        if (peer.player) result.push_back(peer.player.get());
    }
    return result;
}

void HostSession::broadcastReliable(const std::vector<uint8_t>& data, const std::string& excludePeerId) {
    std::vector<std::shared_ptr<PeerChannel>> targets;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [peerId, peer] : m_peers) {
            if (peerId == excludePeerId) continue;
            if (peer.channel && peer.channel->isConnected()) {
                targets.push_back(peer.channel);
            }
        }
    }

    for (auto& ch : targets) {
        ch->sendReliable(data);
    }
}

void HostSession::broadcastUnreliable(const std::vector<uint8_t>& data, const std::string& excludePeerId) {
    std::vector<std::shared_ptr<PeerChannel>> targets;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [peerId, peer] : m_peers) {
            if (peerId == excludePeerId) continue;
            if (peer.channel && peer.channel->isConnected()) {
                targets.push_back(peer.channel);
            }
        }
    }

    for (auto& ch : targets) {
        ch->sendUnreliable(data);
    }
}

bool HostSession::collectPeerForRemoval(const std::string& remotePeerId, DisconnectInfo& out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_peers.find(remotePeerId);
    if (it == m_peers.end()) return false;

    out.peerId = remotePeerId;
    out.id = it->second.id;
    out.channel = it->second.channel;
    m_peers.erase(it);
    return true;
}

void HostSession::finalizePeerRemoval(DisconnectInfo info) {
    std::cerr << "[HOST] Removing peer " << info.peerId << " (id=" << (int)info.id << ")" << std::endl;
    if (info.channel) {
        info.channel->onConnected(nullptr);
        info.channel->onReliableMessage(nullptr);
        info.channel->onUnreliableMessage(nullptr);
        info.channel->close();
    }

    PlayerLeaveMsg leaveMsg;
    leaveMsg.id = info.id;
    broadcastReliable(leaveMsg.serialize());

    int count = 1;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        count = static_cast<int>(m_peers.size()) + 1;
    }
    m_firebase.patch("rooms/" + m_roomId, json{{"playerCount", count}});
}

void HostSession::removePeer(const std::string& remotePeerId) {
    DisconnectInfo info;
    if (!collectPeerForRemoval(remotePeerId, info)) return;
    finalizePeerRemoval(std::move(info));
}

int HostSession::playerCount() const {
    return static_cast<int>(m_peers.size()) + 1;
}

void HostSession::close() {
    if (!m_active) return;
    m_active = false;

    std::vector<std::shared_ptr<PeerChannel>> channelsToClose;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [peerId, peer] : m_peers) {
            if (peer.channel) channelsToClose.push_back(peer.channel);
        }
        m_peers.clear();
    }

    for (auto& ch : channelsToClose) {
        if (ch) {
            ch->onConnected(nullptr);
            ch->onReliableMessage(nullptr);
            ch->onUnreliableMessage(nullptr);
            ch->close();
        }
    }

    m_signaling.stop();
    m_signaling.cleanup();

    m_firebase.del("rooms/" + m_roomId);
    m_firebase.del("signaling/" + m_roomId);
    std::cerr << "[HOST] Session closed, room " << m_roomId << " deleted" << std::endl;
}
