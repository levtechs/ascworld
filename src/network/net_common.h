#pragma once
#include "core/vec_math.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

// Max players per room
static constexpr int MAX_PLAYERS = 8;

// Network update rate
static constexpr float NET_SEND_RATE = 20.0f; // Hz
static constexpr float NET_SEND_INTERVAL = 1.0f / NET_SEND_RATE;

// Peer ID: unique identifier for each player in a session
using PeerId = uint8_t;
static constexpr PeerId PEER_ID_INVALID = 255;
static constexpr PeerId PEER_ID_HOST = 0;

// Network message types (first byte of every message)
enum class NetMsgType : uint8_t {
    PlayerState = 1,    // Position/orientation update (unreliable)
    PlayerJoin = 2,     // New player joined (reliable)
    PlayerLeave = 3,    // Player left (reliable)
    WorldSeed = 4,      // Host sends world seed to new client (reliable)
    ChatMessage = 5,    // Text chat (reliable)
    Ping = 6,           // Latency measurement
    Pong = 7,           // Latency response
    AssignId = 8,       // Host assigns peer ID to client (reliable)
};

// Player network state — sent at NET_SEND_RATE over unreliable channel
// Binary format: [type(1)] [id(1)] [x(4)] [y(4)] [z(4)] [yaw(4)] [pitch(4)] [flags(1)] = 23 bytes
struct PlayerNetState {
    PeerId id = PEER_ID_INVALID;
    float x = 0, y = 0, z = 0;
    float yaw = 0, pitch = 0;
    uint8_t flags = 0; // bit 0: crouching, bit 1: onGround

    Vec3 position() const { return {x, y, z}; }
    void setPosition(const Vec3& p) { x = p.x; y = p.y; z = p.z; }

    static constexpr size_t SERIALIZED_SIZE = 23;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(SERIALIZED_SIZE);
        buf[0] = static_cast<uint8_t>(NetMsgType::PlayerState);
        buf[1] = id;
        std::memcpy(&buf[2], &x, 4);
        std::memcpy(&buf[6], &y, 4);
        std::memcpy(&buf[10], &z, 4);
        std::memcpy(&buf[14], &yaw, 4);
        std::memcpy(&buf[18], &pitch, 4);
        buf[22] = flags;
        return buf;
    }

    static bool deserialize(const uint8_t* data, size_t len, PlayerNetState& out) {
        if (len < SERIALIZED_SIZE) return false;
        if (data[0] != static_cast<uint8_t>(NetMsgType::PlayerState)) return false;
        out.id = data[1];
        std::memcpy(&out.x, &data[2], 4);
        std::memcpy(&out.y, &data[6], 4);
        std::memcpy(&out.z, &data[10], 4);
        std::memcpy(&out.yaw, &data[14], 4);
        std::memcpy(&out.pitch, &data[18], 4);
        out.flags = data[22];
        return true;
    }
};

// Room info for lobby display
struct RoomInfo {
    std::string roomId;
    std::string name;
    std::string hostName;
    uint32_t seed = 0;
    int playerCount = 0;
    int maxPlayers = MAX_PLAYERS;
    bool isPublic = true;
    bool hasPassword = false;
};

// Player join message: [type(1)] [id(1)] [nameLen(1)] [name(N)]
struct PlayerJoinMsg {
    PeerId id = PEER_ID_INVALID;
    std::string name;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf;
        buf.push_back(static_cast<uint8_t>(NetMsgType::PlayerJoin));
        buf.push_back(id);
        uint8_t nameLen = static_cast<uint8_t>(std::min(name.size(), (size_t)255));
        buf.push_back(nameLen);
        buf.insert(buf.end(), name.begin(), name.begin() + nameLen);
        return buf;
    }

    static bool deserialize(const uint8_t* data, size_t len, PlayerJoinMsg& out) {
        if (len < 3) return false;
        if (data[0] != static_cast<uint8_t>(NetMsgType::PlayerJoin)) return false;
        out.id = data[1];
        uint8_t nameLen = data[2];
        if (len < 3u + nameLen) return false;
        out.name = std::string(reinterpret_cast<const char*>(&data[3]), nameLen);
        return true;
    }
};

// Player leave message: [type(1)] [id(1)]
struct PlayerLeaveMsg {
    PeerId id = PEER_ID_INVALID;

    std::vector<uint8_t> serialize() const {
        return { static_cast<uint8_t>(NetMsgType::PlayerLeave), id };
    }

    static bool deserialize(const uint8_t* data, size_t len, PlayerLeaveMsg& out) {
        if (len < 2) return false;
        if (data[0] != static_cast<uint8_t>(NetMsgType::PlayerLeave)) return false;
        out.id = data[1];
        return true;
    }
};

// World seed message: [type(1)] [seed(4)]
struct WorldSeedMsg {
    uint32_t seed = 0;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(5);
        buf[0] = static_cast<uint8_t>(NetMsgType::WorldSeed);
        std::memcpy(&buf[1], &seed, 4);
        return buf;
    }

    static bool deserialize(const uint8_t* data, size_t len, WorldSeedMsg& out) {
        if (len < 5) return false;
        if (data[0] != static_cast<uint8_t>(NetMsgType::WorldSeed)) return false;
        std::memcpy(&out.seed, &data[1], 4);
        return true;
    }
};

// Assign ID message: [type(1)] [id(1)]
struct AssignIdMsg {
    PeerId id = PEER_ID_INVALID;

    std::vector<uint8_t> serialize() const {
        return { static_cast<uint8_t>(NetMsgType::AssignId), id };
    }

    static bool deserialize(const uint8_t* data, size_t len, AssignIdMsg& out) {
        if (len < 2) return false;
        if (data[0] != static_cast<uint8_t>(NetMsgType::AssignId)) return false;
        out.id = data[1];
        return true;
    }
};

// Ping/Pong message: [type(1)] [timestamp(8)]
struct PingPongMsg {
    NetMsgType type = NetMsgType::Ping;
    int64_t timestamp = 0;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(9);
        buf[0] = static_cast<uint8_t>(type);
        std::memcpy(&buf[1], &timestamp, 8);
        return buf;
    }

    static bool deserialize(const uint8_t* data, size_t len, PingPongMsg& out) {
        if (len < 9) return false;
        if (data[0] != static_cast<uint8_t>(NetMsgType::Ping) &&
            data[0] != static_cast<uint8_t>(NetMsgType::Pong)) return false;
        out.type = static_cast<NetMsgType>(data[0]);
        std::memcpy(&out.timestamp, &data[1], 8);
        return true;
    }
};

// Helper: get message type from raw data
inline NetMsgType getMessageType(const uint8_t* data, size_t len) {
    if (len == 0) return static_cast<NetMsgType>(0);
    return static_cast<NetMsgType>(data[0]);
}
