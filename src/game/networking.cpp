#include "game/networking.h"
#include "game/firebase_client.h"
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <rtc/rtc.hpp>
#include <rtc/peerconnection.hpp>
#include <rtc/datachannel.hpp>
#include <rtc/description.hpp>

using json = nlohmann::json;

// Debug log to file since stderr may be captured by terminal raw mode
static std::ofstream& netlog() {
    static std::ofstream f("/tmp/netlog_" + std::to_string(getpid()) + ".log", std::ios::app);
    return f;
}
#define NETLOG(msg) do { netlog() << msg; netlog().flush(); } while(0)

NetworkingManager::NetworkingManager(const std::string& clientUUID, const std::string& playerName)
    : m_clientUUID(clientUUID), m_playerName(playerName) {
    
    NETLOG("[NET] NetworkingManager created, uuid=" << clientUUID);
    rtc::InitLogger(rtc::LogLevel::Warning);
    m_rtcConfig.iceServers.emplace_back("stun:stun.l.google.com:19302");

    m_signalingThread = std::thread(&NetworkingManager::runSignalingLoop, this);
}

void NetworkingManager::refreshLobbies() {
    NETLOG("[Networking] Refreshing lobbies...");
    auto lobbiesJson = FirebaseClient::get("lobbies");
    NETLOG("[Networking] Firebase returned: " << lobbiesJson.dump());
    
    m_discoveredLobbies.clear();
    if (lobbiesJson.is_object()) {
        for (auto& [uuid, data] : lobbiesJson.items()) {
            if (!data.is_object()) continue;
            
            LobbyInfo info;
            info.uuid = uuid;
            info.name = data.value("name", "Unknown");
            info.hostName = data.value("hostName", "Unknown");
            info.playerCount = data.value("playerCount", 0);
            info.lastSeen = data.value("lastSeen", (int64_t)0);
            
            int64_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            if (now - info.lastSeen < 15) {
                m_discoveredLobbies.push_back(info);
            }
        }
    }
}

NetworkingManager::~NetworkingManager() {
    m_running = false;
    m_sigCV.notify_all();
    if (m_signalingThread.joinable()) m_signalingThread.join();
    
    // Clean up Firebase presence
    if (m_isPublic) {
        FirebaseClient::remove("lobbies/" + m_lobbyUUID);
        FirebaseClient::remove("signaling/" + m_lobbyUUID);
    }
    
    std::lock_guard<std::mutex> lock(m_peersMutex);
    m_peers.clear();
}

// --- Unified game actions (loopback for host, network for client) ---

void NetworkingManager::reportDamage(const std::string& victimUUID, float amount, const std::string& attackerUUID) {
    if (m_mode == Mode::Host) {
        // Loopback: apply damage directly through HostAuthority
        m_hostAuthority.applyDamage(victimUUID, amount, attackerUUID);
    } else {
        // Client: send damage report to remote host
        sendDamageReport(victimUUID, amount, attackerUUID);
    }
}

void NetworkingManager::requestRespawn(const std::string& uuid) {
    if (m_mode == Mode::Host) {
        m_hostAuthority.respawnPlayer(uuid);
    } else {
        sendRespawnRequest();
    }
}

void NetworkingManager::pickupItem(const std::string& playerUUID, uint16_t entityId) {
    if (m_mode == Mode::Host) {
        m_hostAuthority.pickupItem(playerUUID, entityId);
    } else {
        sendPickupRequest(entityId);
    }
}

void NetworkingManager::dropItem(const std::string& playerUUID, int slot, const Vec3& dropPos) {
    if (m_mode == Mode::Host) {
        m_hostAuthority.dropItem(playerUUID, slot, dropPos);
    } else {
        sendDropRequest(slot, dropPos);
    }
}

void NetworkingManager::useItem(const std::string& playerUUID, int slot) {
    if (m_mode == Mode::Host) {
        m_hostAuthority.useItem(playerUUID, slot);
    } else {
        sendUseItemRequest(slot);
    }
}

void NetworkingManager::update(float dt, RootState& state) {
    // Host always processes, even with no peers (was: early return for Offline+!public)
    bool hasNetwork = m_isPublic || m_mode == Mode::Client || peerCount() > 0;

    if (hasNetwork) {
        // Send keepalives every 1 second
        m_keepaliveTimer += dt;
        if (m_keepaliveTimer >= 1.0f) {
            m_keepaliveTimer = 0.0f;
            std::lock_guard<std::mutex> lock(m_peersMutex);
            rtc::binary ping = { static_cast<std::byte>(PacketType::Keepalive) };
            for (auto& [uuid, peer] : m_peers) {
                if (peer->ready && peer->dc && peer->dc->isOpen()) {
                    try { peer->dc->send(ping); } catch (...) {}
                }
            }
        }

        // Clean up closed/timed-out peers and detect host disconnect
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            bool hostPeerLost = false;
            for (auto it = m_peers.begin(); it != m_peers.end(); ) {
                auto pcState = it->second->pc->state();
                bool iceDead = (pcState == rtc::PeerConnection::State::Closed ||
                                pcState == rtc::PeerConnection::State::Failed);
                // App-level timeout: no packet received in 2 seconds (2 missed keepalives)
                if (it->second->ready) it->second->timeSinceLastRecv += dt;
                bool appTimeout = it->second->ready && it->second->timeSinceLastRecv > 2.0f;

                if (iceDead || appTimeout) {
                    NETLOG("[Networking] Removing peer: " << it->first
                           << " iceDead=" << iceDead << " appTimeout=" << appTimeout);
                    if (m_mode == Mode::Client && it->first == m_lobbyUUID) {
                        hostPeerLost = true;
                    }
                    it = m_peers.erase(it);
                    m_heartbeatNeeded = true;
                } else {
                    ++it;
                }
            }

            // Host migration: if we're a client and lost connection to host
            if (hostPeerLost) {
                NETLOG("[Networking] Host disconnected! Promoting self to host.");
                m_originalLobbyUUID = m_lobbyUUID;
                m_mode = Mode::Host;
                m_lobbyUUID = m_clientUUID;
                state.world.hostUUID = m_clientUUID;
                m_promotedToHost = true;
                m_isPublic = true;
                try { FirebaseClient::remove("lobbies/" + m_originalLobbyUUID); } catch (...) {}
                try { FirebaseClient::remove("signaling/" + m_originalLobbyUUID); } catch (...) {}
            }
        }

        // Process incoming packets on main thread
        std::vector<InboundPacket> packetsToProcess;
        {
            std::lock_guard<std::mutex> pktLock(m_packetMutex);
            packetsToProcess = std::move(m_packetQueue);
            m_packetQueue.clear();
        }

        for (const auto& pkt : packetsToProcess) {
            if (m_mode == Mode::Client && pkt.type == PacketType::DeltaUpdate) {
                if (pkt.payload.contains("players")) {
                    for (auto& [uuid, pState] : pkt.payload["players"].items()) {
                        if (uuid != m_clientUUID) {
                            state.players[uuid] = pState.get<PlayerState>();
                        } else {
                            auto hostPs = pState.get<PlayerState>();
                            auto& localPs = state.players[m_clientUUID];
                            bool wasDead = localPs.isDead;
                            bool nowDead = hostPs.isDead;
                            bool stateChanged = (wasDead != nowDead);
                            // Always accept host-authoritative fields
                            localPs.health = hostPs.health;
                            localPs.kills = hostPs.kills;
                            localPs.deaths = hostPs.deaths;
                            localPs.isDead = hostPs.isDead;
                            // Inventory is host-authoritative (pickup/drop/death go through host)
                            localPs.inventory = hostPs.inventory;
                            localPs.selectedSlot = hostPs.selectedSlot;
                            // On death/respawn transitions, also accept position
                            if (stateChanged) {
                                localPs.position = hostPs.position;
                                localPs.yaw = hostPs.yaw;
                                localPs.pitch = hostPs.pitch;
                                localPs.activeItemType = hostPs.activeItemType;
                                localPs.attackProgress = hostPs.attackProgress;
                            }
                        }
                    }
                }
                if (pkt.payload.contains("entities")) {
                    state.entities = pkt.payload["entities"].get<std::vector<WorldEntity>>();
                }
                if (pkt.payload.contains("metadata")) {
                    auto& md = pkt.payload["metadata"];
                    if (md.contains("name")) state.metadata.name = md["name"].get<std::string>();
                    if (md.contains("timestamp")) state.metadata.timestamp = md["timestamp"].get<int64_t>();
                }
            } else if (m_mode == Mode::Client && pkt.type == PacketType::FullState) {
                try {
                    auto newState = pkt.payload.get<RootState>();
                    state = newState;
                    NETLOG("[Networking] Received full state from host (seed=" << state.world.seed << ")");
                } catch (const std::exception& e) {
                    NETLOG("[Networking] Failed to parse full state: " << e.what());
                }
            } else if (m_mode == Mode::Host && pkt.type == PacketType::DeltaUpdate) {
                if (pkt.payload.contains("player")) {
                    auto clientPs = pkt.payload["player"].get<PlayerState>();
                    bool isNew = (state.players.find(pkt.peerUUID) == state.players.end());
                    auto& hostPs = state.players[pkt.peerUUID];
                    if (isNew) {
                        hostPs.health = 100.0f;
                        hostPs.isDead = false;
                        hostPs.kills = 0;
                        hostPs.deaths = 0;
                        hostPs.setDefaultLoadout();
                    }
                    // Accept identity/visual fields always (for rendering death body)
                    hostPs.uuid = clientPs.uuid;
                    hostPs.name = clientPs.name;
                    hostPs.appearance = clientPs.appearance;
                    // Dead players stay dead until they send a RespawnRequest.
                    // Don't accept gameplay fields while dead (prevents inventory dupe,
                    // health reset, and position desync).
                    if (hostPs.isDead) continue;
                    // Accept client-controlled gameplay fields
                    hostPs.position = clientPs.position;
                    hostPs.yaw = clientPs.yaw;
                    hostPs.pitch = clientPs.pitch;
                    hostPs.selectedSlot = clientPs.selectedSlot;
                    hostPs.gameTime = clientPs.gameTime;
                    hostPs.activeItemType = clientPs.activeItemType;
                    hostPs.attackProgress = clientPs.attackProgress;
                    // inventory is now host-authoritative (modified via pickup/drop/death)
                }
            } else if (m_mode == Mode::Host && pkt.type == PacketType::RespawnRequest) {
                NETLOG("[Networking] RespawnRequest from " << pkt.peerUUID);
                m_respawnQueue.push_back(pkt.peerUUID);
            } else if (m_mode == Mode::Host && pkt.type == PacketType::DamageReport) {
                std::string victim = pkt.payload.value("victim", "");
                std::string attacker = pkt.payload.value("attacker", "");
                float amount = pkt.payload.value("amount", 0.0f);
                m_hostAuthority.applyDamage(victim, amount, attacker);
            } else if (m_mode == Mode::Host && pkt.type == PacketType::PickupRequest) {
                uint16_t entityId = pkt.payload.value("entityId", (uint16_t)0);
                NETLOG("[Networking] PickupRequest from " << pkt.peerUUID << " entity=" << entityId);
                m_hostAuthority.pickupItem(pkt.peerUUID, entityId);
            } else if (m_mode == Mode::Host && pkt.type == PacketType::DropRequest) {
                int slot = pkt.payload.value("slot", -1);
                Vec3 pos;
                if (pkt.payload.contains("pos")) pos = pkt.payload["pos"].get<Vec3>();
                NETLOG("[Networking] DropRequest from " << pkt.peerUUID << " slot=" << slot);
                m_hostAuthority.dropItem(pkt.peerUUID, slot, pos);
            } else if (m_mode == Mode::Host && pkt.type == PacketType::UseItemRequest) {
                int slot = pkt.payload.value("slot", -1);
                NETLOG("[Networking] UseItemRequest from " << pkt.peerUUID << " slot=" << slot);
                m_hostAuthority.useItem(pkt.peerUUID, slot);
            }
        }
    }

    // Host: process queued death/respawn events from remote clients
    if (m_mode == Mode::Host) {
        {
            std::vector<std::string> deaths;
            {
                std::lock_guard<std::mutex> lock(m_packetMutex);
                deaths.swap(m_deathQueue);
            }
            m_hostAuthority.processDeathQueue(deaths);
        }
        {
            std::vector<std::string> respawns;
            {
                std::lock_guard<std::mutex> lock(m_packetMutex);
                respawns.swap(m_respawnQueue);
            }
            m_hostAuthority.processRespawnQueue(respawns);
        }
    }

    // Network send
    if (hasNetwork) {
        if (m_mode == Mode::Host) {
            broadcastDelta(state);

            std::lock_guard<std::mutex> lock(m_peersMutex);
            for (auto& [uuid, peer] : m_peers) {
                if (peer->ready && !peer->fullStateSent) {
                    NETLOG("[Networking] Sending full state to new peer: " << uuid);
                    sendFullState(*peer, state);
                    peer->fullStateSent = true;
                }
            }
        } else if (m_mode == Mode::Client) {
            sendClientState(state);
        }
    }
}

void NetworkingManager::runSignalingLoop() {
    auto lastHeartbeat = std::chrono::steady_clock::now();
    
    while (m_running) {
        if (!m_running) break;
        
        // Only do signaling work if we're networked (public host or client)
        if (m_isPublic || m_mode == Mode::Client) {
            try {
                if (m_running) handleSignaling();
            } catch (const std::exception& e) {
                NETLOG("[Networking] Signaling error: " << e.what());
            }
            
            if (m_running && m_mode == Mode::Host && m_isPublic) {
                auto now = std::chrono::steady_clock::now();
                bool needsHeartbeat = m_heartbeatNeeded.exchange(false);
                if (needsHeartbeat || std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat).count() >= 5) {
                    lastHeartbeat = now;
                    json lobby;
                    lobby["name"] = m_lobbyName;
                    lobby["hostName"] = m_playerName;
                    
                    {
                        std::lock_guard<std::mutex> lock(m_peersMutex);
                        lobby["playerCount"] = (int)m_peers.size() + 1;
                    }
                    lobby["lastSeen"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    FirebaseClient::put("lobbies/" + m_lobbyUUID, lobby);
                }
            }
        }
        
        if (!m_running) break;
        std::unique_lock<std::mutex> lock(m_sigMutex);
        // Poll faster during active signaling (connecting), slower when idle
        bool activeSignaling = m_isPublic || m_mode == Mode::Client;
        int pollMs = activeSignaling ? 100 : 500;
        m_sigCV.wait_for(lock, std::chrono::milliseconds(pollMs));
    }
}

void NetworkingManager::makePublic(const std::string& lobbyName, RootState& state) {
    m_lobbyName = lobbyName;
    m_lobbyUUID = m_clientUUID;
    
    m_mode = Mode::Host;
    m_isPublic = true;
    state.world.hostUUID = m_clientUUID;

    json lobby;
    lobby["name"] = m_lobbyName;
    lobby["hostName"] = m_playerName;
    lobby["playerCount"] = 1;
    lobby["lastSeen"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    FirebaseClient::put("lobbies/" + m_lobbyUUID, lobby);
    
    // Clean old signaling data
    FirebaseClient::remove("signaling/" + m_lobbyUUID);
}

void NetworkingManager::makePrivate() {
    if (m_isPublic) {
        FirebaseClient::remove("lobbies/" + m_lobbyUUID);
        FirebaseClient::remove("signaling/" + m_lobbyUUID);
        m_isPublic = false;
    }
    // Stay as Host (always host of own game), just not discoverable
}

void NetworkingManager::joinLobby(const std::string& lobbyUUID) {
    m_lobbyUUID = lobbyUUID;
    m_mode = Mode::Client;
    
    // Set up peer immediately - stale signaling cleanup happens in background
    setupPeer(lobbyUUID, true);
}

void NetworkingManager::disconnect() {
    if (m_isPublic) {
        m_isPublic = false;
        try {
            FirebaseClient::remove("lobbies/" + m_lobbyUUID);
            FirebaseClient::remove("signaling/" + m_lobbyUUID);
        } catch (...) {}
    }
    // Return to local host mode (host of own game, not discoverable)
    m_mode = Mode::Host;
    std::lock_guard<std::mutex> lock(m_peersMutex);
    m_peers.clear();
}

void NetworkingManager::setupPeer(const std::string& peerUUID, bool isOffer) {
    auto peer = std::make_unique<Peer>();
    peer->uuid = peerUUID;
    peer->pc = std::make_shared<rtc::PeerConnection>(m_rtcConfig);

    peer->pc->onStateChange([uuid = peerUUID](rtc::PeerConnection::State state) {
        NETLOG("[Networking] PC(" << uuid.substr(0,8) << ") state: " << (int)state);
    });

    peer->pc->onGatheringStateChange([uuid = peerUUID](rtc::PeerConnection::GatheringState state) {
        NETLOG("[Networking] PC(" << uuid.substr(0,8) << ") gathering: " << (int)state);
    });

    // Trickle ICE: queue each candidate as it arrives
    peer->pc->onLocalCandidate([p = peer.get()](rtc::Candidate candidate) {
        std::string candidateStr = std::string(candidate);
        std::string mid = candidate.mid();
        NETLOG("[Networking] Local ICE candidate: " << candidateStr.substr(0, 60) << "...");
        
        json cand;
        cand["candidate"] = candidateStr;
        cand["mid"] = mid;
        
        std::lock_guard<std::mutex> lock(p->candidateMutex);
        p->pendingLocalCandidates.push_back(cand.dump());
    });

    if (isOffer) {
        peer->dc = peer->pc->createDataChannel("game-data");
        peer->dc->onOpen([&, p = peer.get()]() {
            NETLOG("[Networking] DataChannel OPEN to " << p->uuid.substr(0,8));
            p->ready = true;
        });
        peer->dc->onClosed([uuid = peerUUID]() {
            NETLOG("[Networking] DataChannel CLOSED to " << uuid.substr(0,8));
        });
        peer->dc->onMessage([&, p = peer.get()](rtc::message_variant data) {
            handlePacket(p->uuid, data);
        });
        
        peer->pc->setLocalDescription(); // Generates offer SDP
    } else {
        peer->pc->onDataChannel([&, p = peer.get()](std::shared_ptr<rtc::DataChannel> dc) {
            NETLOG("[Networking] Received DataChannel from " << p->uuid.substr(0,8));
            p->dc = dc;
            p->dc->onOpen([p]() { 
                NETLOG("[Networking] DataChannel OPEN (host side) to " << p->uuid.substr(0,8));
                p->ready = true; 
            });
            p->dc->onClosed([uuid = p->uuid]() {
                NETLOG("[Networking] DataChannel CLOSED (host side) to " << uuid.substr(0,8));
            });
            p->dc->onMessage([&, p](rtc::message_variant data) {
                handlePacket(p->uuid, data);
            });
        });
    }

    std::lock_guard<std::mutex> lock(m_peersMutex);
    m_peers[peerUUID] = std::move(peer);
}

void NetworkingManager::handleSignaling() {
    if (m_mode == Mode::Host) {
        // 1. Check for new offers
        auto offers = FirebaseClient::get("signaling/" + m_lobbyUUID + "/offers");
        if (offers.is_object()) {
            for (auto& [peerUUID, offerVal] : offers.items()) {
                bool isNew = false;
                {
                    std::lock_guard<std::mutex> lock(m_peersMutex);
                    isNew = (m_peers.find(peerUUID) == m_peers.end());
                }
                
                if (isNew && offerVal.is_string()) {
                    NETLOG("[SIG-H] New peer: " << peerUUID.substr(0,8));
                    setupPeer(peerUUID, false);
                    
                    std::lock_guard<std::mutex> lock(m_peersMutex);
                    auto& p = m_peers[peerUUID];
                    p->pc->setRemoteDescription(rtc::Description(offerVal.get<std::string>(), "offer"));
                    p->remoteDescriptionSet = true;
                    p->pc->setLocalDescription();
                }
            }
        }
        
        // 2. Send our answer SDP
        std::string answerToSend;
        std::string answerPeerUUID;
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            for (auto& [peerUUID, peer] : m_peers) {
                if (!peer->descriptionSent && peer->remoteDescriptionSet && peer->pc->localDescription()) {
                    answerToSend = std::string(peer->pc->localDescription().value());
                    answerPeerUUID = peerUUID;
                    peer->descriptionSent = true;
                    break;
                }
            }
        }
        if (!answerToSend.empty()) {
            NETLOG("[SIG-H] Sending answer to " << answerPeerUUID.substr(0,8));
            FirebaseClient::put("signaling/" + m_lobbyUUID + "/answers/" + answerPeerUUID, answerToSend);
        }
        
        // 3. Push local ICE candidates (batched as single PATCH)
        struct CandBatch { std::string peerUUID; std::deque<std::string> candidates; int startIdx; };
        std::vector<CandBatch> hostCandBatches;
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            for (auto& [peerUUID, peer] : m_peers) {
                std::deque<std::string> toSend;
                {
                    std::lock_guard<std::mutex> cLock(peer->candidateMutex);
                    toSend.swap(peer->pendingLocalCandidates);
                }
                if (!toSend.empty()) {
                    hostCandBatches.push_back({peerUUID, std::move(toSend), peer->localCandidateIndex});
                    peer->localCandidateIndex += (int)hostCandBatches.back().candidates.size();
                }
            }
        }
        for (auto& batch : hostCandBatches) {
            NETLOG("[SIG-H] Pushing " << batch.candidates.size() << " candidates for " << batch.peerUUID.substr(0,8));
            json candPatch;
            int idx = batch.startIdx;
            for (auto& c : batch.candidates) {
                candPatch[std::to_string(idx)] = c;
                idx++;
            }
            FirebaseClient::patch("signaling/" + m_lobbyUUID + "/host_candidates/" + batch.peerUUID, candPatch);
        }
        
        // 4. Pull remote ICE candidates
        struct PeerCandInfo { std::string peerUUID; int startIdx; };
        std::vector<PeerCandInfo> peersToCheck;
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            for (auto& [peerUUID, peer] : m_peers) {
                if (peer->remoteDescriptionSet) {
                    peersToCheck.push_back({peerUUID, peer->remoteCandidateIndex});
                }
            }
        }
        for (auto& info : peersToCheck) {
            auto candidates = FirebaseClient::get("signaling/" + m_lobbyUUID + "/candidates/" + info.peerUUID);
            if (candidates.is_array() && (int)candidates.size() > info.startIdx) {
                std::lock_guard<std::mutex> lock(m_peersMutex);
                if (m_peers.find(info.peerUUID) != m_peers.end()) {
                    auto& peer = m_peers[info.peerUUID];
                    for (int i = peer->remoteCandidateIndex; i < (int)candidates.size(); i++) {
                        try {
                            auto candObj = json::parse(candidates[i].get<std::string>());
                            peer->pc->addRemoteCandidate(rtc::Candidate(candObj["candidate"].get<std::string>(), candObj["mid"].get<std::string>()));
                        } catch (const std::exception& e) {
                            NETLOG("[SIG-H] Failed to add candidate: " << e.what());
                        }
                        peer->remoteCandidateIndex = i + 1;
                    }
                }
            }
        }
    } else if (m_mode == Mode::Client) {
        
        // 1. Send our offer SDP
        std::string offerToSend;
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            if (m_peers.find(m_lobbyUUID) != m_peers.end()) {
                auto& p = m_peers[m_lobbyUUID];
                if (!p->descriptionSent && p->pc->localDescription()) {
                    offerToSend = std::string(p->pc->localDescription().value());
                    p->descriptionSent = true;
                }
            }
        }
        if (!offerToSend.empty()) {
            NETLOG("[SIG-C] Cleaning stale data and sending offer...");
            json sigPatch;
            sigPatch["offers/" + m_clientUUID] = offerToSend;
            sigPatch["answers/" + m_clientUUID] = nullptr;
            sigPatch["candidates/" + m_clientUUID] = nullptr;
            sigPatch["host_candidates/" + m_clientUUID] = nullptr;
            FirebaseClient::patch("signaling/" + m_lobbyUUID, sigPatch);
            NETLOG("[SIG-C] Offer sent!");
        }
        
        // 2. Check for answer
        bool needAnswer = false;
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            if (m_peers.find(m_lobbyUUID) != m_peers.end()) {
                auto& p = m_peers[m_lobbyUUID];
                if (p->descriptionSent && !p->remoteDescriptionSet) {
                    needAnswer = true;
                }
            }
        }
        if (needAnswer) {
            auto answer = FirebaseClient::get("signaling/" + m_lobbyUUID + "/answers/" + m_clientUUID);
            if (answer.is_string()) {
                std::lock_guard<std::mutex> lock(m_peersMutex);
                if (m_peers.find(m_lobbyUUID) != m_peers.end()) {
                    auto& p = m_peers[m_lobbyUUID];
                    if (!p->remoteDescriptionSet) {
                        NETLOG("[SIG-C] Setting remote description (answer, len=" << answer.get<std::string>().size() << ")");
                        p->pc->setRemoteDescription(rtc::Description(answer.get<std::string>(), "answer"));
                        p->remoteDescriptionSet = true;
                        NETLOG("[SIG-C] Remote description set!");
                    }
                }
            }
        }
        
        // 3. Push local ICE candidates (batched as single PATCH)
        std::deque<std::string> clientCandsToSend;
        int clientCandStartIdx = 0;
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            if (m_peers.find(m_lobbyUUID) != m_peers.end()) {
                auto& p = m_peers[m_lobbyUUID];
                std::lock_guard<std::mutex> cLock(p->candidateMutex);
                clientCandsToSend.swap(p->pendingLocalCandidates);
                clientCandStartIdx = p->localCandidateIndex;
                p->localCandidateIndex += (int)clientCandsToSend.size();
            }
        }
        if (!clientCandsToSend.empty()) {
            NETLOG("[SIG-C] Pushing " << clientCandsToSend.size() << " candidates...");
            json candPatch;
            int idx = clientCandStartIdx;
            for (auto& c : clientCandsToSend) {
                candPatch[std::to_string(idx)] = c;
                idx++;
            }
            FirebaseClient::patch("signaling/" + m_lobbyUUID + "/candidates/" + m_clientUUID, candPatch);
        }
        
        // 4. Pull host's ICE candidates
        NETLOG("[SIG-C] Step 4: pulling host ICE candidates...");
        bool hasRemoteDesc = false;
        int candStartIdx = 0;
        {
            std::lock_guard<std::mutex> lock(m_peersMutex);
            if (m_peers.find(m_lobbyUUID) != m_peers.end()) {
                auto& p = m_peers[m_lobbyUUID];
                hasRemoteDesc = p->remoteDescriptionSet;
                candStartIdx = p->remoteCandidateIndex;

            }
        }
        if (hasRemoteDesc) {
            auto candidates = FirebaseClient::get("signaling/" + m_lobbyUUID + "/host_candidates/" + m_clientUUID);
            if (candidates.is_array() && (int)candidates.size() > candStartIdx) {
                std::lock_guard<std::mutex> lock(m_peersMutex);
                if (m_peers.find(m_lobbyUUID) != m_peers.end()) {
                    auto& p = m_peers[m_lobbyUUID];
                    for (int i = p->remoteCandidateIndex; i < (int)candidates.size(); i++) {
                        try {
                            auto candObj = json::parse(candidates[i].get<std::string>());
                            p->pc->addRemoteCandidate(rtc::Candidate(candObj["candidate"].get<std::string>(), candObj["mid"].get<std::string>()));
                        } catch (const std::exception& e) {
                            NETLOG("[SIG] Failed to add candidate: " << e.what());
                        }
                        p->remoteCandidateIndex = i + 1;
                    }
                }
            }
        }
    }
}

void NetworkingManager::handlePacket(const std::string& peerUUID, rtc::message_variant data) {
    if (std::holds_alternative<std::string>(data)) return;
    
    auto& bin = std::get<rtc::binary>(data);
    if (bin.empty()) return;
    
    // Reset keepalive timer for this peer on any packet
    {
        std::lock_guard<std::mutex> lock(m_peersMutex);
        auto it = m_peers.find(peerUUID);
        if (it != m_peers.end()) it->second->timeSinceLastRecv = 0.0f;
    }
    
    PacketType type = static_cast<PacketType>(bin[0]);
    
    // Keepalive packets have no payload - just reset the timer above
    if (type == PacketType::Keepalive) return;
    
    // Some packet types (RespawnRequest) have no payload
    if (bin.size() <= 1) {
        std::lock_guard<std::mutex> lock(m_packetMutex);
        m_packetQueue.push_back({peerUUID, type, json{}});
        return;
    }
    
    std::vector<uint8_t> payload;
    payload.reserve(bin.size() - 1);
    for (size_t i = 1; i < bin.size(); ++i) {
        payload.push_back(static_cast<uint8_t>(bin[i]));
    }
    
    try {
        json j = json::from_cbor(payload);
        std::lock_guard<std::mutex> lock(m_packetMutex);
        m_packetQueue.push_back({peerUUID, type, std::move(j)});
    } catch (const std::exception& e) {
        NETLOG("[Networking] Failed to decode packet: " << e.what());
    }
}

void NetworkingManager::broadcastDelta(const RootState& state) {
    std::lock_guard<std::mutex> lock(m_peersMutex);
    if (m_peers.empty()) return;

    json delta;
    delta["players"] = state.players;
    delta["entities"] = state.entities;
    delta["metadata"] = json{{"name", state.metadata.name}, {"timestamp", state.metadata.timestamp}};
    
    std::vector<uint8_t> cbor = json::to_cbor(delta);
    rtc::binary packet;
    packet.push_back(static_cast<std::byte>(PacketType::DeltaUpdate));
    for (uint8_t b : cbor) packet.push_back(static_cast<std::byte>(b));

    for (auto& [uuid, peer] : m_peers) {
        if (peer->ready && peer->dc && peer->dc->isOpen()) {
            try {
                peer->dc->send(packet);
            } catch (const std::exception& e) {
                NETLOG("[Networking] Send failed to " << uuid.substr(0,8) << ": " << e.what());
            }
        }
    }
}

bool NetworkingManager::wasPromotedToHost() {
    return m_promotedToHost.exchange(false);
}

int NetworkingManager::peerCount() const {
    int count = 0;
    for (auto& [uuid, peer] : m_peers) {
        if (peer->ready) count++;
    }
    return count;
}

std::unordered_set<std::string> NetworkingManager::activePeerUUIDs() const {
    std::unordered_set<std::string> result;
    for (auto& [uuid, peer] : m_peers) {
        if (peer->ready) result.insert(uuid);
    }
    return result;
}

int NetworkingManager::getPing(const std::string& uuid) const {
    auto it = m_peers.find(uuid);
    return it != m_peers.end() ? it->second->lastPing : 0;
}

void NetworkingManager::sendClientState(const RootState& state) {
    std::lock_guard<std::mutex> lock(m_peersMutex);
    if (m_peers.empty()) return;

    auto it = state.players.find(m_clientUUID);
    if (it == state.players.end()) return;

    json delta;
    delta["player"] = it->second;
    
    std::vector<uint8_t> cbor = json::to_cbor(delta);
    rtc::binary packet;
    packet.push_back(static_cast<std::byte>(PacketType::DeltaUpdate));
    for (uint8_t b : cbor) packet.push_back(static_cast<std::byte>(b));

    auto peerIt = m_peers.find(m_lobbyUUID);
    if (peerIt != m_peers.end() && peerIt->second->ready && peerIt->second->dc && peerIt->second->dc->isOpen()) {
        try {
            peerIt->second->dc->send(packet);
        } catch (const std::exception& e) {
            NETLOG("[Networking] Send failed: " << e.what());
        }
    }
}

void NetworkingManager::sendDamageReport(const std::string& victimUUID, float amount, const std::string& attackerUUID) {
    json report;
    report["victim"] = victimUUID;
    report["attacker"] = attackerUUID;
    report["amount"] = amount;
    
    std::vector<uint8_t> cbor = json::to_cbor(report);
    rtc::binary packet;
    packet.push_back(static_cast<std::byte>(PacketType::DamageReport));
    for (uint8_t b : cbor) packet.push_back(static_cast<std::byte>(b));

    std::lock_guard<std::mutex> lock(m_peersMutex);
    // Send to host
    auto it = m_peers.find(m_lobbyUUID);
    if (it != m_peers.end() && it->second->ready && it->second->dc && it->second->dc->isOpen()) {
        try { it->second->dc->send(packet); } catch (...) {}
    }
}

void NetworkingManager::sendRespawnRequest() {
    rtc::binary packet;
    packet.push_back(static_cast<std::byte>(PacketType::RespawnRequest));
    
    std::lock_guard<std::mutex> lock(m_peersMutex);
    auto it = m_peers.find(m_lobbyUUID);
    if (it != m_peers.end() && it->second->ready && it->second->dc && it->second->dc->isOpen()) {
        try { it->second->dc->send(packet); } catch (...) {}
    }
}

void NetworkingManager::sendPickupRequest(uint16_t entityId) {
    json payload;
    payload["entityId"] = entityId;

    std::vector<uint8_t> cbor = json::to_cbor(payload);
    rtc::binary packet;
    packet.push_back(static_cast<std::byte>(PacketType::PickupRequest));
    for (uint8_t b : cbor) packet.push_back(static_cast<std::byte>(b));

    std::lock_guard<std::mutex> lock(m_peersMutex);
    auto it = m_peers.find(m_lobbyUUID);
    if (it != m_peers.end() && it->second->ready && it->second->dc && it->second->dc->isOpen()) {
        try { it->second->dc->send(packet); } catch (...) {}
    }
}

void NetworkingManager::sendDropRequest(int slot, const Vec3& dropPos) {
    json payload;
    payload["slot"] = slot;
    payload["pos"] = dropPos;

    std::vector<uint8_t> cbor = json::to_cbor(payload);
    rtc::binary packet;
    packet.push_back(static_cast<std::byte>(PacketType::DropRequest));
    for (uint8_t b : cbor) packet.push_back(static_cast<std::byte>(b));

    std::lock_guard<std::mutex> lock(m_peersMutex);
    auto it = m_peers.find(m_lobbyUUID);
    if (it != m_peers.end() && it->second->ready && it->second->dc && it->second->dc->isOpen()) {
        try { it->second->dc->send(packet); } catch (...) {}
    }
}

void NetworkingManager::sendUseItemRequest(int slot) {
    json payload;
    payload["slot"] = slot;

    std::vector<uint8_t> cbor = json::to_cbor(payload);
    rtc::binary packet;
    packet.push_back(static_cast<std::byte>(PacketType::UseItemRequest));
    for (uint8_t b : cbor) packet.push_back(static_cast<std::byte>(b));

    std::lock_guard<std::mutex> lock(m_peersMutex);
    auto it = m_peers.find(m_lobbyUUID);
    if (it != m_peers.end() && it->second->ready && it->second->dc && it->second->dc->isOpen()) {
        try { it->second->dc->send(packet); } catch (...) {}
    }
}

void NetworkingManager::sendFullState(Peer& peer, const RootState& state) {
    if (!peer.ready || !peer.dc || !peer.dc->isOpen()) return;
    
    json j = state;
    std::vector<uint8_t> cbor = json::to_cbor(j);
    rtc::binary packet;
    packet.push_back(static_cast<std::byte>(PacketType::FullState));
    for (uint8_t b : cbor) packet.push_back(static_cast<std::byte>(b));
    
    try {
        peer.dc->send(packet);
    } catch (const std::exception& e) {
        NETLOG("[Networking] SendFullState failed: " << e.what());
    }
}
