#pragma once
#include "network/signaling.h"
#include "network/net_common.h"
#include <rtc/rtc.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>

class PeerChannel {
public:
    PeerChannel();
    ~PeerChannel();

    // Create a new peer connection with STUN servers
    void create(bool isOfferer);

    // Close the connection
    void close();

    // Set signaling callbacks — these are called when SDP/ICE need to be sent
    using SDPCallback = std::function<void(const std::string& sdp)>;
    using CandidateCallback = std::function<void(const std::string& candidate,
                                                  const std::string& mid)>;
    void onLocalDescription(SDPCallback callback);
    void onLocalCandidate(CandidateCallback callback);

    // Apply remote SDP and ICE candidates
    void setRemoteDescription(const std::string& sdp, const std::string& type);
    void addRemoteCandidate(const std::string& candidate, const std::string& mid);

    // Data channel callbacks
    using MessageCallback = std::function<void(const uint8_t* data, size_t len)>;
    using StateCallback = std::function<void(bool connected)>;

    void onReliableMessage(MessageCallback callback);
    void onUnreliableMessage(MessageCallback callback);
    void onConnected(StateCallback callback);

    // Send data
    void sendReliable(const std::vector<uint8_t>& data);
    void sendReliable(const uint8_t* data, size_t len);
    void sendUnreliable(const std::vector<uint8_t>& data);
    void sendUnreliable(const uint8_t* data, size_t len);

    // State
    bool isConnected() const { return m_connected; }

private:
    std::shared_ptr<rtc::PeerConnection> m_pc;
    std::shared_ptr<rtc::DataChannel> m_reliableDC;
    std::shared_ptr<rtc::DataChannel> m_unreliableDC;

    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_reliableOpen{false};
    std::atomic<bool> m_unreliableOpen{false};
    bool m_connectedFired{false}; // guards one-shot connected callback

    SDPCallback m_sdpCallback;
    CandidateCallback m_candidateCallback;
    MessageCallback m_reliableMessageCb;
    MessageCallback m_unreliableMessageCb;
    StateCallback m_connectedCb;

    // Buffered events (in case they fire before callbacks are set)
    std::string m_pendingSdp;
    std::vector<std::pair<std::string, std::string>> m_pendingCandidates;

    // Buffer remote candidates until remote description is set
    bool m_hasRemoteDescription{false};
    std::vector<std::pair<std::string, std::string>> m_pendingRemoteCandidates;

    std::mutex m_mutex;

    void setupDataChannel(std::shared_ptr<rtc::DataChannel> dc, bool reliable);
};
