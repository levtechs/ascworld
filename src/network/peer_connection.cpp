#include "network/peer_connection.h"
#include <stdexcept>

PeerChannel::PeerChannel() = default;

PeerChannel::~PeerChannel() {
    close();
}

void PeerChannel::create(bool isOfferer) {
    // Configure STUN servers for NAT traversal
    rtc::Configuration config;
    config.iceServers.emplace_back("stun:stun.l.google.com:19302");
    config.iceServers.emplace_back("stun:stun1.l.google.com:19302");

    m_pc = std::make_shared<rtc::PeerConnection>(config);

    // --- Signaling callbacks ---

    m_pc->onLocalDescription([this](rtc::Description description) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_sdpCallback) {
            m_sdpCallback(std::string(description));
        } else {
            m_pendingSdp = std::string(description);
        }
    });

    m_pc->onLocalCandidate([this](rtc::Candidate candidate) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_candidateCallback) {
            m_candidateCallback(std::string(candidate), candidate.mid());
        } else {
            m_pendingCandidates.emplace_back(std::string(candidate), candidate.mid());
        }
    });

    // --- Connection state ---

    m_pc->onStateChange([this](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Connected) {
            // Don't fire connected callback here — wait for DataChannels to open
            m_connected = true;
        } else if (state == rtc::PeerConnection::State::Disconnected ||
                   state == rtc::PeerConnection::State::Failed ||
                   state == rtc::PeerConnection::State::Closed) {
            bool wasConnected = m_connected.exchange(false);
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_connectedCb && wasConnected) {
                m_connectedCb(false);
            }
        }
    });

    // --- Data channel handling ---

    if (isOfferer) {
        // Offerer creates the data channels

        // Reliable channel (default — ordered, reliable)
        rtc::DataChannelInit reliableInit;
        m_reliableDC = m_pc->createDataChannel("reliable", reliableInit);
        setupDataChannel(m_reliableDC, true);

        // Unreliable channel (unordered, no retransmits — fire-and-forget like UDP)
        rtc::DataChannelInit unreliableInit;
        unreliableInit.reliability.unordered = true;
        unreliableInit.reliability.maxRetransmits = 0;
        m_unreliableDC = m_pc->createDataChannel("unreliable", unreliableInit);
        setupDataChannel(m_unreliableDC, false);
    } else {
        // Answerer waits for remote data channels
        m_pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> dc) {
            std::string label = dc->label();
            if (label == "reliable") {
                m_reliableDC = dc;
                setupDataChannel(dc, true);
            } else if (label == "unreliable") {
                m_unreliableDC = dc;
                setupDataChannel(dc, false);
            }
        });
    }
}

void PeerChannel::setupDataChannel(std::shared_ptr<rtc::DataChannel> dc, bool reliable) {
    dc->onOpen([this, reliable]() {
        if (reliable) {
            m_reliableOpen = true;
        } else {
            m_unreliableOpen = true;
        }

        // Fire connected callback only when BOTH channels are open
        if (m_reliableOpen.load() && m_unreliableOpen.load()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_connectedCb && !m_connectedFired) {
                m_connectedFired = true;
                m_connectedCb(true);
            }
        }
    });

    dc->onClosed([this, reliable]() {
        if (reliable) m_reliableOpen = false;
        else m_unreliableOpen = false;
    });

    dc->onMessage([this, reliable](auto data) {
        // libdatachannel onMessage provides either string or binary via variant
        std::lock_guard<std::mutex> lock(m_mutex);

        if (std::holds_alternative<rtc::binary>(data)) {
            const auto& bin = std::get<rtc::binary>(data);
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(bin.data());
            size_t len = bin.size();

            if (reliable) {
                if (m_reliableMessageCb) m_reliableMessageCb(ptr, len);
            } else {
                if (m_unreliableMessageCb) m_unreliableMessageCb(ptr, len);
            }
        } else if (std::holds_alternative<std::string>(data)) {
            // Treat string messages as binary
            const auto& str = std::get<std::string>(data);
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(str.data());
            size_t len = str.size();

            if (reliable) {
                if (m_reliableMessageCb) m_reliableMessageCb(ptr, len);
            } else {
                if (m_unreliableMessageCb) m_unreliableMessageCb(ptr, len);
            }
        }
    });
}

void PeerChannel::close() {
    m_connected = false;
    m_reliableOpen = false;
    m_unreliableOpen = false;
    m_connectedFired = false;

    if (m_reliableDC) {
        try {
            m_reliableDC->close();
        } catch (...) {}
        m_reliableDC.reset();
    }

    if (m_unreliableDC) {
        try {
            m_unreliableDC->close();
        } catch (...) {}
        m_unreliableDC.reset();
    }

    if (m_pc) {
        try {
            m_pc->close();
        } catch (...) {}
        m_pc.reset();
    }
}

void PeerChannel::onLocalDescription(SDPCallback callback) {
    std::string buffered;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sdpCallback = callback;
        buffered = std::move(m_pendingSdp);
        m_pendingSdp.clear();
    }
    // Replay buffered SDP outside lock
    if (!buffered.empty() && callback) {
        callback(buffered);
    }
}

void PeerChannel::onLocalCandidate(CandidateCallback callback) {
    std::vector<std::pair<std::string, std::string>> buffered;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_candidateCallback = callback;
        buffered = std::move(m_pendingCandidates);
        m_pendingCandidates.clear();
    }
    // Replay buffered candidates outside lock
    if (!buffered.empty() && callback) {
        for (auto& [cand, mid] : buffered) {
            callback(cand, mid);
        }
    }
}

void PeerChannel::onReliableMessage(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_reliableMessageCb = std::move(callback);
}

void PeerChannel::onUnreliableMessage(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_unreliableMessageCb = std::move(callback);
}

void PeerChannel::onConnected(StateCallback callback) {
    bool bothOpen = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connectedCb = callback;
        bothOpen = m_reliableOpen.load() && m_unreliableOpen.load() && !m_connectedFired;
        if (bothOpen) m_connectedFired = true;
    }
    // If both DataChannels already open when callback is set, fire immediately
    if (bothOpen && callback) {
        callback(true);
    }
}

void PeerChannel::setRemoteDescription(const std::string& sdp, const std::string& type) {
    if (!m_pc) {
        return;
    }

    rtc::Description::Type descType;
    if (type == "offer") {
        descType = rtc::Description::Type::Offer;
    } else {
        descType = rtc::Description::Type::Answer;
    }

    try {
        m_pc->setRemoteDescription(rtc::Description(sdp, descType));
    } catch (...) {
        return;
    }

    // Flush any buffered remote candidates
    std::vector<std::pair<std::string, std::string>> buffered;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_hasRemoteDescription = true;
        buffered = std::move(m_pendingRemoteCandidates);
        m_pendingRemoteCandidates.clear();
    }
    if (!buffered.empty()) {
        for (auto& [cand, mid2] : buffered) {
            try {
                m_pc->addRemoteCandidate(rtc::Candidate(cand, mid2));
            } catch (...) {
            }
        }
    }
}

void PeerChannel::addRemoteCandidate(const std::string& candidate, const std::string& mid) {
    if (!m_pc) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_hasRemoteDescription) {
            m_pendingRemoteCandidates.emplace_back(candidate, mid);
            return;
        }
    }

    try {
        m_pc->addRemoteCandidate(rtc::Candidate(candidate, mid));
    } catch (...) {
    }
}

void PeerChannel::sendReliable(const std::vector<uint8_t>& data) {
    sendReliable(data.data(), data.size());
}

void PeerChannel::sendReliable(const uint8_t* data, size_t len) {
    if (!m_reliableDC || !m_reliableDC->isOpen()) return;
    try {
        m_reliableDC->send(reinterpret_cast<const std::byte*>(data), len);
    } catch (...) {}
}

void PeerChannel::sendUnreliable(const std::vector<uint8_t>& data) {
    sendUnreliable(data.data(), data.size());
}

void PeerChannel::sendUnreliable(const uint8_t* data, size_t len) {
    if (!m_unreliableDC || !m_unreliableDC->isOpen()) return;
    try {
        m_unreliableDC->send(reinterpret_cast<const std::byte*>(data), len);
    } catch (...) {}
}
