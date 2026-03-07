#include "network/lobby.h"
#include <iostream>

Lobby::Lobby() = default;

Lobby::~Lobby() {
    disconnect();
}

bool Lobby::init() {
#ifndef FIREBASE_URL
    return false;
#else
    m_firebase.init(FIREBASE_URL);
    return m_firebase.isInitialized();
#endif
}

std::vector<RoomInfo> Lobby::refreshRooms() {
    // Always use a fresh lightweight client session for listing so we never keep
    // stale in-memory room vectors from an active joined client instance.
    ClientSession tempClient(m_firebase);
    auto rooms = tempClient.listRooms();
    std::cerr << "[LOBBY] refreshRooms returned " << rooms.size() << " room(s)" << std::endl;
    return rooms;
}

std::string Lobby::hostGame(const std::string& roomName, uint32_t seed,
                             bool isPublic, const std::string& password) {
    // Disconnect any existing session
    disconnect();

    m_hostSession = std::make_unique<HostSession>(m_firebase);
    std::string roomId = m_hostSession->createRoom(roomName, m_playerName, seed, isPublic, password);

    if (roomId.empty()) {
        m_hostSession.reset();
        return "";
    }

    m_hostSession->startAccepting();
    return roomId;
}

bool Lobby::joinGame(const std::string& roomId, const std::string& password) {
    // Disconnect any existing session
    disconnect();

    m_clientSession = std::make_unique<ClientSession>(m_firebase);
    if (!m_clientSession->joinRoom(roomId, m_playerName, password)) {
        m_clientSession.reset();
        return false;
    }

    return true;
}

void Lobby::update(float dt, const PlayerNetState& localState) {
    if (m_hostSession) {
        m_hostSession->update(dt, localState);
    } else if (m_clientSession) {
        m_clientSession->update(dt, localState);
    }
}

std::vector<RemotePlayer*> Lobby::getRemotePlayers() {
    if (m_hostSession) {
        return m_hostSession->getRemotePlayers();
    } else if (m_clientSession) {
        return m_clientSession->getRemotePlayers();
    }
    return {};
}

void Lobby::disconnect() {
    if (m_hostSession) {
        m_hostSession->close();
        m_hostSession.reset();
    }
    if (m_clientSession) {
        m_clientSession->leave();
        m_clientSession.reset();
    }
}

uint32_t Lobby::worldSeed() const {
    if (m_hostSession) {
        return m_hostSession->seed();
    } else if (m_clientSession) {
        return m_clientSession->worldSeed();
    }
    return 0;
}

bool Lobby::hasWorldSeed() const {
    if (m_hostSession) {
        return true; // host always has the seed
    } else if (m_clientSession) {
        return m_clientSession->hasWorldSeed();
    }
    return false;
}

PeerId Lobby::localPeerId() const {
    if (m_hostSession) {
        return PEER_ID_HOST;
    } else if (m_clientSession) {
        return m_clientSession->localPeerId();
    }
    return PEER_ID_INVALID;
}

ClientSession::State Lobby::clientState() const {
    if (m_clientSession) {
        return m_clientSession->state();
    }
    if (m_hostSession) {
        return ClientSession::State::Connected; // host is always "connected"
    }
    return ClientSession::State::Disconnected;
}
