#pragma once
#include "network/firebase_client.h"
#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include <unordered_map>

// Handles WebRTC signaling through Firebase RTDB
// Firebase paths used:
//   /signaling/{roomId}/{peerId}/offer   — SDP offer (JSON string)
//   /signaling/{roomId}/{peerId}/answer  — SDP answer (JSON string)
//   /signaling/{roomId}/{peerId}/candidates/{index} — ICE candidates
class Signaling {
public:
    Signaling(FirebaseClient& firebase);

    // Set the room and local peer ID for signaling
    void setRoom(const std::string& roomId, const std::string& localPeerId);

    // --- Host side ---
    // Listen for incoming offers from clients
    // Callback: (remotePeerId, sdpOffer)
    using OfferCallback = std::function<void(const std::string& remotePeerId,
                                              const std::string& sdpOffer)>;
    void listenForOffers(OfferCallback callback);

    // Send an SDP answer to a specific peer
    void sendAnswer(const std::string& remotePeerId, const std::string& sdpAnswer);

    // --- Client side ---
    // Send an SDP offer to the host
    void sendOffer(const std::string& hostPeerId, const std::string& sdpOffer);

    // Listen for the answer from the host
    // Callback: (sdpAnswer)
    using AnswerCallback = std::function<void(const std::string& sdpAnswer)>;
    void listenForAnswer(const std::string& hostPeerId, AnswerCallback callback);

    // --- ICE candidates (both sides) ---
    // Send an ICE candidate
    void sendCandidate(const std::string& remotePeerId, const std::string& candidate,
                       const std::string& mid, int sdpMLineIndex);

    // Listen for ICE candidates from a remote peer
    using CandidateCallback = std::function<void(const std::string& candidate,
                                                  const std::string& mid,
                                                  int sdpMLineIndex)>;
    void listenForCandidates(const std::string& remotePeerId, CandidateCallback callback);

    // Cleanup signaling data for this session
    void cleanup();

    // Stop all listeners
    void stop();

private:
    FirebaseClient& m_firebase;
    std::string m_roomId;
    std::string m_localPeerId;

    // Track candidate indices for sending
    std::mutex m_mutex;
    std::unordered_map<std::string, int> m_candidateCounters; // remotePeerId -> count

    // Track polling threads
    std::atomic<bool> m_running{true};

    // Track known peers for offer listening
    std::mutex m_offerMutex;
    std::unordered_map<std::string, bool> m_knownOfferPeers;

    // Build signaling path
    std::string sigPath(const std::string& peerId) const;
};
