#include "network/signaling.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

using json = nlohmann::json;

Signaling::Signaling(FirebaseClient& firebase)
    : m_firebase(firebase) {}

void Signaling::setRoom(const std::string& roomId, const std::string& localPeerId) {
    m_roomId = roomId;
    m_localPeerId = localPeerId;
}

std::string Signaling::sigPath(const std::string& peerId) const {
    return "signaling/" + m_roomId + "/" + peerId;
}

// --- Host side ---

void Signaling::listenForOffers(OfferCallback callback) {
    std::string basePath = "signaling/" + m_roomId;

    m_firebase.startListening(basePath, [this, callback](const std::string& event,
                                                          const std::string& path,
                                                          const json& data) {
        if (!m_running) return;

        // SSE events come with paths relative to the listened path.
        // We expect paths like "/{peerId}/offer" for individual updates
        // or "/" for the initial data dump.

        if (event == "put") {
            // Initial data dump at root: data is an object of {peerId: {offer: ..., ...}}
            if (path == "/") {
                if (data.is_object()) {
                    for (auto& [peerId, peerData] : data.items()) {
                        if (peerId == m_localPeerId) continue;
                        if (peerData.is_object() && peerData.contains("offer") &&
                            peerData["offer"].is_string()) {
                            std::lock_guard<std::mutex> lock(m_offerMutex);
                            if (m_knownOfferPeers.find(peerId) == m_knownOfferPeers.end()) {
                                m_knownOfferPeers[peerId] = true;
                                callback(peerId, peerData["offer"].get<std::string>());
                            }
                        }
                    }
                }
                return;
            }

            // Path format: "/{peerId}/offer"
            // Parse the peer ID and field from the path
            if (path.size() > 1) {
                // Remove leading '/'
                std::string subPath = path.substr(1);
                size_t slashPos = subPath.find('/');
                if (slashPos != std::string::npos) {
                    std::string peerId = subPath.substr(0, slashPos);
                    std::string field = subPath.substr(slashPos + 1);

                    if (peerId != m_localPeerId && field == "offer" && data.is_string()) {
                        std::lock_guard<std::mutex> lock(m_offerMutex);
                        if (m_knownOfferPeers.find(peerId) == m_knownOfferPeers.end()) {
                            m_knownOfferPeers[peerId] = true;
                            callback(peerId, data.get<std::string>());
                        }
                    }
                } else {
                    // Path is just "/{peerId}" — data is the entire peer object
                    std::string peerId = subPath;
                    if (peerId != m_localPeerId && data.is_object() &&
                        data.contains("offer") && data["offer"].is_string()) {
                        std::lock_guard<std::mutex> lock(m_offerMutex);
                        if (m_knownOfferPeers.find(peerId) == m_knownOfferPeers.end()) {
                            m_knownOfferPeers[peerId] = true;
                            callback(peerId, data["offer"].get<std::string>());
                        }
                    }
                }
            }
        }
    });
}

void Signaling::sendAnswer(const std::string& remotePeerId, const std::string& sdpAnswer) {
    std::string path = sigPath(remotePeerId) + "/answer";
    m_firebase.put(path, json(sdpAnswer));
}

// --- Client side ---

void Signaling::sendOffer(const std::string& hostPeerId, const std::string& sdpOffer) {
    (void)hostPeerId; // Offer is stored under our own peer ID
    std::string path = sigPath(m_localPeerId) + "/offer";
    m_firebase.put(path, json(sdpOffer));
}

void Signaling::listenForAnswer(const std::string& hostPeerId, AnswerCallback callback) {
    (void)hostPeerId;
    std::string answerPath = sigPath(m_localPeerId) + "/answer";

    // Poll in a detached thread every 200ms until answer appears
    std::thread([this, answerPath, callback]() {
        while (m_running) {
            json data = m_firebase.get(answerPath);
            if (!data.is_null() && data.is_string()) {
                std::string answer = data.get<std::string>();
                if (!answer.empty()) {
                    callback(answer);
                    return;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }).detach();
}

// --- ICE candidates ---

void Signaling::sendCandidate(const std::string& remotePeerId, const std::string& candidate,
                               const std::string& mid, int sdpMLineIndex) {
    int index;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        index = m_candidateCounters[remotePeerId]++;
    }

    json candidateJson = {
        {"candidate", candidate},
        {"mid", mid},
        {"sdpMLineIndex", sdpMLineIndex}
    };

    std::string path = sigPath(m_localPeerId) + "/candidates/" + std::to_string(index);
    m_firebase.put(path, candidateJson);
}

void Signaling::listenForCandidates(const std::string& remotePeerId, CandidateCallback callback) {
    std::string candidatesPath = sigPath(remotePeerId) + "/candidates";

    // Poll in a detached thread every 200ms for new candidates
    std::thread([this, candidatesPath, callback]() {
        int processedCount = 0;

        while (m_running) {
            json data = m_firebase.get(candidatesPath);

            if (!data.is_null()) {
                // Data can be an object with string keys "0", "1", ... or an array
                if (data.is_object()) {
                    for (auto& [key, val] : data.items()) {
                        int idx = -1;
                        try {
                            idx = std::stoi(key);
                        } catch (...) {
                            continue;
                        }
                        if (idx >= processedCount && val.is_object()) {
                            std::string candidate = val.value("candidate", "");
                            std::string mid = val.value("mid", "");
                            int sdpMLineIndex = val.value("sdpMLineIndex", 0);
                            if (!candidate.empty()) {
                                callback(candidate, mid, sdpMLineIndex);
                            }
                        }
                    }
                    // Update processed count to the highest index + 1
                    for (auto& [key, val] : data.items()) {
                        try {
                            int idx = std::stoi(key);
                            if (idx + 1 > processedCount) {
                                processedCount = idx + 1;
                            }
                        } catch (...) {}
                    }
                } else if (data.is_array()) {
                    for (size_t i = static_cast<size_t>(processedCount); i < data.size(); ++i) {
                        const auto& val = data[i];
                        if (val.is_object()) {
                            std::string candidate = val.value("candidate", "");
                            std::string mid = val.value("mid", "");
                            int sdpMLineIndex = val.value("sdpMLineIndex", 0);
                            if (!candidate.empty()) {
                                callback(candidate, mid, sdpMLineIndex);
                            }
                        }
                    }
                    processedCount = static_cast<int>(data.size());
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }).detach();
}

// --- Cleanup ---

void Signaling::cleanup() {
    m_firebase.del("signaling/" + m_roomId);
}

void Signaling::stop() {
    m_running = false;
    m_firebase.stopListening();
}
