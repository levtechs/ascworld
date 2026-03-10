#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <variant>
#include "core/vec_math.h"
#include "game/player_state.h"
#include "game/entity.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// --- JSON Serialization Helpers ---

// Vec3
inline void to_json(json& j, const Vec3& v) {
    j = json{{"x", v.x}, {"y", v.y}, {"z", v.z}};
}
inline void from_json(const json& j, Vec3& v) {
    v.x = j.at("x").get<float>();
    v.y = j.at("y").get<float>();
    v.z = j.at("z").get<float>();
}

// CharacterAppearance
inline void to_json(json& j, const CharacterAppearance& a) {
    j = json{{"colorIndex", a.colorIndex}, {"design", static_cast<uint8_t>(a.design)}};
}
inline void from_json(const json& j, CharacterAppearance& a) {
    a.colorIndex = j.at("colorIndex").get<uint8_t>();
    a.design = static_cast<CharacterDesign>(j.at("design").get<uint8_t>());
}

// ItemSlot
inline void to_json(json& j, const ItemSlot& s) {
    j = json{{"type", static_cast<uint8_t>(s.type)}, {"count", s.count}};
}
inline void from_json(const json& j, ItemSlot& s) {
    s.type = static_cast<ItemType>(j.at("type").get<uint8_t>());
    s.count = j.at("count").get<int>();
}

// PlayerState
inline void to_json(json& j, const PlayerState& p) {
    j = json{
        {"uuid", p.uuid},
        {"name", p.name},
        {"position", p.position},
        {"yaw", p.yaw},
        {"pitch", p.pitch},
        {"health", p.health},
        {"kills", p.kills},
        {"deaths", p.deaths},
        {"isDead", p.isDead},
        {"deathTime", p.deathTime},
        {"inventory", p.inventory},
        {"selectedSlot", p.selectedSlot},
        {"appearance", p.appearance},
        {"gameTime", p.gameTime},
        {"activeItem", static_cast<uint8_t>(p.activeItemType)},
        {"atkProg", p.attackProgress}
    };
}
inline void from_json(const json& j, PlayerState& p) {
    p.uuid = j.at("uuid").get<std::string>();
    p.name = j.at("name").get<std::string>();
    p.position = j.at("position").get<Vec3>();
    p.yaw = j.at("yaw").get<float>();
    p.pitch = j.at("pitch").get<float>();
    p.health = j.at("health").get<float>();
    p.kills = j.at("kills").get<int>();
    p.deaths = j.at("deaths").get<int>();
    p.isDead = j.at("isDead").get<bool>();
    if (j.contains("deathTime")) p.deathTime = j.at("deathTime").get<float>();
    p.inventory = j.at("inventory").get<std::array<ItemSlot, 9>>();
    p.selectedSlot = j.at("selectedSlot").get<int>();
    p.appearance = j.at("appearance").get<CharacterAppearance>();
    p.gameTime = j.at("gameTime").get<float>();
    if (j.contains("activeItem")) p.activeItemType = static_cast<ItemType>(j.at("activeItem").get<uint8_t>());
    if (j.contains("atkProg")) p.attackProgress = j.at("atkProg").get<float>();
}

// EntityData and WorldEntity
inline void to_json(json& j, const WorldEntity& e) {
    j = json{
        {"id", e.id},
        {"type", static_cast<uint8_t>(e.type)},
        {"ownerId", e.ownerId},
        {"position", e.position},
        {"velocity", e.velocity},
        {"yaw", e.yaw},
        {"lifetime", e.lifetime},
        {"spawnTime", e.spawnTime},
        {"flags", e.flags},
        {"hitMask", e.hitMask}
    };

    switch (e.type) {
        case EntityType::DroppedItem:
            if (auto* d = std::get_if<DroppedItemData>(&e.data)) {
                j["data"] = json{{"itemType", static_cast<uint8_t>(d->itemType)}, {"count", d->count}};
            }
            break;
        case EntityType::Projectile:
            if (auto* d = std::get_if<ProjectileData>(&e.data)) {
                j["data"] = json{{"sourceType", static_cast<uint8_t>(d->sourceType)}};
            }
            break;
        case EntityType::LaserBeam:
            if (auto* d = std::get_if<LaserBeamData>(&e.data)) {
                j["data"] = json{{"endPoint", d->endPoint}, {"damage", d->damage}};
            }
            break;
        case EntityType::SaberSwing:
            if (auto* d = std::get_if<SaberSwingData>(&e.data)) {
                j["data"] = json{
                    {"forward", d->forward}, {"range", d->range},
                    {"damage", d->damage}, {"coneCos", d->coneCos}
                };
            }
            break;
        case EntityType::Explosion:
            if (auto* d = std::get_if<ExplosionData>(&e.data)) {
                j["data"] = json{{"maxRadius", d->maxRadius}, {"damage", d->damage}, {"duration", d->duration}};
            }
            break;
        default: break;
    }
}

inline void from_json(const json& j, WorldEntity& e) {
    e.id = j.at("id").get<uint16_t>();
    e.type = static_cast<EntityType>(j.at("type").get<uint8_t>());
    e.ownerId = j.at("ownerId").get<uint8_t>();
    e.position = j.at("position").get<Vec3>();
    e.velocity = j.at("velocity").get<Vec3>();
    e.yaw = j.at("yaw").get<float>();
    e.lifetime = j.at("lifetime").get<float>();
    e.spawnTime = j.at("spawnTime").get<float>();
    e.flags = j.at("flags").get<uint8_t>();
    e.hitMask = j.at("hitMask").get<uint8_t>();

    if (j.contains("data")) {
        const auto& d = j.at("data");
        switch (e.type) {
            case EntityType::DroppedItem: {
                DroppedItemData data;
                data.itemType = static_cast<ItemType>(d.at("itemType").get<uint8_t>());
                data.count = d.at("count").get<uint8_t>();
                e.data = data;
                break;
            }
            case EntityType::Projectile: {
                ProjectileData data;
                data.sourceType = static_cast<ItemType>(d.at("sourceType").get<uint8_t>());
                e.data = data;
                break;
            }
            case EntityType::LaserBeam: {
                LaserBeamData data;
                data.endPoint = d.at("endPoint").get<Vec3>();
                data.damage = d.at("damage").get<float>();
                e.data = data;
                break;
            }
            case EntityType::SaberSwing: {
                SaberSwingData data;
                data.forward = d.at("forward").get<Vec3>();
                data.range = d.at("range").get<float>();
                data.damage = d.at("damage").get<float>();
                data.coneCos = d.at("coneCos").get<float>();
                e.data = data;
                break;
            }
            case EntityType::Explosion: {
                ExplosionData data;
                data.maxRadius = d.at("maxRadius").get<float>();
                data.damage = d.at("damage").get<float>();
                data.duration = d.at("duration").get<float>();
                e.data = data;
                break;
            }
            default: e.data = std::monostate{}; break;
        }
    }
}

// --- RootState hierarchy ---

struct RootState {
    struct Metadata {
        std::string name;
        int64_t timestamp = 0;
        uint32_t version = 1;
    } metadata;

    struct WorldData {
        uint32_t seed = 0;
        float gameTime = 0.0f;
        std::string hostUUID;
        int64_t lastSyncTime = 0;
    } world;

    std::unordered_map<std::string, PlayerState> players;
    std::vector<WorldEntity> entities;
};

// RootState serialization
inline void to_json(json& j, const RootState::Metadata& m) {
    j = json{{"name", m.name}, {"timestamp", m.timestamp}, {"version", m.version}};
}
inline void from_json(const json& j, RootState::Metadata& m) {
    m.name = j.at("name").get<std::string>();
    m.timestamp = j.at("timestamp").get<int64_t>();
    m.version = j.at("version").get<uint32_t>();
}

inline void to_json(json& j, const RootState::WorldData& w) {
    j = json{{"seed", w.seed}, {"gameTime", w.gameTime}, {"hostUUID", w.hostUUID}, {"lastSyncTime", w.lastSyncTime}};
}
inline void from_json(const json& j, RootState::WorldData& w) {
    w.seed = j.at("seed").get<uint32_t>();
    w.gameTime = j.at("gameTime").get<float>();
    w.hostUUID = j.value("hostUUID", "");
    w.lastSyncTime = j.value("lastSyncTime", (int64_t)0);
}

inline void to_json(json& j, const RootState& r) {
    j = json{
        {"metadata", r.metadata},
        {"world", r.world},
        {"players", r.players},
        {"entities", r.entities}
    };
}
inline void from_json(const json& j, RootState& r) {
    r.metadata = j.at("metadata").get<RootState::Metadata>();
    r.world = j.at("world").get<RootState::WorldData>();
    r.players = j.at("players").get<std::unordered_map<std::string, PlayerState>>();
    r.entities = j.at("entities").get<std::vector<WorldEntity>>();
}
