#pragma once
#include "core/vec_math.h"
#include "game/entity.h"
#include "game/item.h"
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
    EntitySpawn = 20,   // Spawn world entity (reliable)
    EntityRemove = 21,  // Remove world entity by id (reliable)
    DamageEvent = 22,   // Host applies damage to a player (reliable)
    HealthState = 23,   // Authoritative player health sync (reliable)
};

// Player network state — sent at NET_SEND_RATE over unreliable channel
// Binary format: [type(1)] [id(1)] [x(4)] [y(4)] [z(4)] [yaw(4)] [pitch(4)] [flags(1)] = 23 bytes
struct PlayerNetState {
    PeerId id = PEER_ID_INVALID;
    float x = 0, y = 0, z = 0;
    float yaw = 0, pitch = 0;
    uint8_t flags = 0; // bit 0: crouching, bit 1: onGround, bits 2-4: activeWeaponType (ItemType 0-7), bit 5: isAttacking

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

// Player join message: [type(1)] [id(1)] [nameLen(1)] [name(N)] [appearance(1)]
// The appearance byte is optional for backward compat — if missing, defaults to 0
struct PlayerJoinMsg {
    PeerId id = PEER_ID_INVALID;
    std::string name;
    uint8_t appearance = 0; // packed CharacterAppearance byte

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf;
        buf.push_back(static_cast<uint8_t>(NetMsgType::PlayerJoin));
        buf.push_back(id);
        uint8_t nameLen = static_cast<uint8_t>(std::min(name.size(), (size_t)255));
        buf.push_back(nameLen);
        buf.insert(buf.end(), name.begin(), name.begin() + nameLen);
        buf.push_back(appearance);
        return buf;
    }

    static bool deserialize(const uint8_t* data, size_t len, PlayerJoinMsg& out) {
        if (len < 3) return false;
        if (data[0] != static_cast<uint8_t>(NetMsgType::PlayerJoin)) return false;
        out.id = data[1];
        uint8_t nameLen = data[2];
        if (len < 3u + nameLen) return false;
        out.name = std::string(reinterpret_cast<const char*>(&data[3]), nameLen);
        // Appearance byte is optional (backward compat)
        if (len >= 3u + nameLen + 1) {
            out.appearance = data[3 + nameLen];
        } else {
            out.appearance = 0;
        }
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

// ---- Flags helpers for PlayerNetState ----
// bits 0-1: crouching, onGround (existing)
// bits 2-4: activeWeaponType (ItemType, 3 bits = values 0-7)
// bit 5:    isAttacking
// bit 6:    isDead

inline uint8_t encodeWeaponFlags(uint8_t existingFlags, ItemType weapon, bool attacking, bool dead = false) {
    uint8_t f = existingFlags & 0x03; // preserve bits 0-1
    f |= (static_cast<uint8_t>(weapon) & 0x07) << 2; // bits 2-4
    if (attacking) f |= (1 << 5); // bit 5
    if (dead) f |= (1 << 6); // bit 6
    return f;
}

inline ItemType decodeWeaponType(uint8_t flags) {
    uint8_t val = (flags >> 2) & 0x07;
    if (val >= static_cast<uint8_t>(ItemType::COUNT)) return ItemType::None;
    return static_cast<ItemType>(val);
}

inline bool decodeIsAttacking(uint8_t flags) {
    return (flags & (1 << 5)) != 0;
}

inline bool decodeIsDead(uint8_t flags) {
    return (flags & (1 << 6)) != 0;
}

// ---- World state network messages ----

// EntitySpawn:
// [type(1)] [id(2)] [entityType(1)] [ownerId(1)]
// [x(4)] [y(4)] [z(4)] [vx(4)] [vy(4)] [vz(4)] [yaw(4)] [lifetime(4)]
// [flags(1)] [hitMask(1)] [dataLen(1)] [data(N)]
struct EntitySpawnMsg {
    WorldEntity entity;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> data = serializeEntityData(entity.type, entity.data);
        uint8_t dataLen = static_cast<uint8_t>(std::min<size_t>(255, data.size()));
        std::vector<uint8_t> buf(40 + dataLen);
        buf[0] = static_cast<uint8_t>(NetMsgType::EntitySpawn);
        std::memcpy(&buf[1], &entity.id, 2);
        buf[3] = static_cast<uint8_t>(entity.type);
        buf[4] = entity.ownerId;
        std::memcpy(&buf[5], &entity.position.x, 4);
        std::memcpy(&buf[9], &entity.position.y, 4);
        std::memcpy(&buf[13], &entity.position.z, 4);
        std::memcpy(&buf[17], &entity.velocity.x, 4);
        std::memcpy(&buf[21], &entity.velocity.y, 4);
        std::memcpy(&buf[25], &entity.velocity.z, 4);
        std::memcpy(&buf[29], &entity.yaw, 4);
        std::memcpy(&buf[33], &entity.lifetime, 4);
        buf[37] = entity.flags;
        buf[38] = entity.hitMask;
        buf[39] = dataLen;
        if (dataLen > 0) {
            std::memcpy(&buf[40], data.data(), dataLen);
        }
        return buf;
    }

    static bool deserialize(const uint8_t* data, size_t len, EntitySpawnMsg& out) {
        if (len < 40) return false;
        if (data[0] != static_cast<uint8_t>(NetMsgType::EntitySpawn)) return false;

        WorldEntity e;
        std::memcpy(&e.id, &data[1], 2);
        e.type = static_cast<EntityType>(data[3]);
        e.ownerId = data[4];
        std::memcpy(&e.position.x, &data[5], 4);
        std::memcpy(&e.position.y, &data[9], 4);
        std::memcpy(&e.position.z, &data[13], 4);
        std::memcpy(&e.velocity.x, &data[17], 4);
        std::memcpy(&e.velocity.y, &data[21], 4);
        std::memcpy(&e.velocity.z, &data[25], 4);
        std::memcpy(&e.yaw, &data[29], 4);
        std::memcpy(&e.lifetime, &data[33], 4);
        e.flags = data[37];
        e.hitMask = data[38];
        uint8_t dataLen = data[39];
        if (len < static_cast<size_t>(40 + dataLen)) return false;
        if (!deserializeEntityData(e.type, &data[40], dataLen, e.data)) return false;
        out.entity = e;
        return true;
    }
};

// EntityRemove: [type(1)] [id(2)]
struct EntityRemoveMsg {
    uint16_t entityId = 0;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(3);
        buf[0] = static_cast<uint8_t>(NetMsgType::EntityRemove);
        std::memcpy(&buf[1], &entityId, 2);
        return buf;
    }

    static bool deserialize(const uint8_t* data, size_t len, EntityRemoveMsg& out) {
        if (len < 3) return false;
        if (data[0] != static_cast<uint8_t>(NetMsgType::EntityRemove)) return false;
        std::memcpy(&out.entityId, &data[1], 2);
        return true;
    }
};

// DamageEvent: [type(1)] [sourceId(1)] [targetId(1)] [amount(4)] [weaponType(1)]
struct DamageEventMsg {
    PeerId sourceId = PEER_ID_INVALID;
    PeerId targetId = PEER_ID_INVALID;
    float amount = 0.0f;
    EntityType weaponType = EntityType::None;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(8);
        buf[0] = static_cast<uint8_t>(NetMsgType::DamageEvent);
        buf[1] = sourceId;
        buf[2] = targetId;
        std::memcpy(&buf[3], &amount, 4);
        buf[7] = static_cast<uint8_t>(weaponType);
        return buf;
    }

    static bool deserialize(const uint8_t* data, size_t len, DamageEventMsg& out) {
        if (len < 7) return false;
        if (data[0] != static_cast<uint8_t>(NetMsgType::DamageEvent)) return false;
        out.sourceId = data[1];
        out.targetId = data[2];
        std::memcpy(&out.amount, &data[3], 4);
        out.weaponType = (len >= 8) ? static_cast<EntityType>(data[7]) : EntityType::None;
        return true;
    }
};

// HealthState: [type(1)] [playerId(1)] [health(4)]
struct HealthStateMsg {
    PeerId playerId = PEER_ID_INVALID;
    float health = 100.0f;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(6);
        buf[0] = static_cast<uint8_t>(NetMsgType::HealthState);
        buf[1] = playerId;
        std::memcpy(&buf[2], &health, 4);
        return buf;
    }

    static bool deserialize(const uint8_t* data, size_t len, HealthStateMsg& out) {
        if (len < 6) return false;
        if (data[0] != static_cast<uint8_t>(NetMsgType::HealthState)) return false;
        out.playerId = data[1];
        std::memcpy(&out.health, &data[2], 4);
        return true;
    }
};
