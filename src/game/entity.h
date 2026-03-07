#pragma once
#include "core/vec_math.h"
#include "game/item.h"
#include <cstdint>
#include <vector>
#include <variant>
#include <cstring>

enum class EntityType : uint8_t {
    None = 0,
    DroppedItem,
    Projectile,
    LaserBeam,
    SaberSwing,
    Explosion,
    COUNT
};

struct DroppedItemData {
    ItemType itemType = ItemType::None;
    uint8_t count = 1;
};

struct ProjectileData {
    ItemType sourceType = ItemType::None;
};

struct LaserBeamData {
    Vec3 endPoint;
    float damage = 0.0f;
};

struct SaberSwingData {
    Vec3 forward;
    float range = 2.2f;
    float damage = 25.0f;
    float coneCos = 0.35f;
};

struct ExplosionData {
    float maxRadius = 3.5f;
    float damage = 45.0f;
    float duration = 0.35f;
};

using EntityData = std::variant<
    std::monostate,
    DroppedItemData,
    ProjectileData,
    LaserBeamData,
    SaberSwingData,
    ExplosionData
>;

struct WorldEntity {
    uint16_t id = 0;
    EntityType type = EntityType::None;
    uint8_t ownerId = 0;
    Vec3 position;
    Vec3 velocity;
    float yaw = 0.0f;
    float lifetime = -1.0f;
    float spawnTime = 0.0f;
    uint8_t flags = 0;
    uint8_t hitMask = 0;
    EntityData data;
};

inline std::vector<uint8_t> serializeEntityData(EntityType type, const EntityData& data) {
    std::vector<uint8_t> out;
    switch (type) {
        case EntityType::DroppedItem: {
            const auto* d = std::get_if<DroppedItemData>(&data);
            if (!d) break;
            out.resize(2);
            out[0] = static_cast<uint8_t>(d->itemType);
            out[1] = d->count;
            break;
        }
        case EntityType::Projectile: {
            const auto* d = std::get_if<ProjectileData>(&data);
            if (!d) break;
            out.resize(1);
            out[0] = static_cast<uint8_t>(d->sourceType);
            break;
        }
        case EntityType::LaserBeam: {
            const auto* d = std::get_if<LaserBeamData>(&data);
            if (!d) break;
            out.resize(16);
            std::memcpy(&out[0], &d->endPoint.x, 4);
            std::memcpy(&out[4], &d->endPoint.y, 4);
            std::memcpy(&out[8], &d->endPoint.z, 4);
            std::memcpy(&out[12], &d->damage, 4);
            break;
        }
        case EntityType::SaberSwing: {
            const auto* d = std::get_if<SaberSwingData>(&data);
            if (!d) break;
            out.resize(24);
            std::memcpy(&out[0], &d->forward.x, 4);
            std::memcpy(&out[4], &d->forward.y, 4);
            std::memcpy(&out[8], &d->forward.z, 4);
            std::memcpy(&out[12], &d->range, 4);
            std::memcpy(&out[16], &d->damage, 4);
            std::memcpy(&out[20], &d->coneCos, 4);
            break;
        }
        case EntityType::Explosion: {
            const auto* d = std::get_if<ExplosionData>(&data);
            if (!d) break;
            out.resize(12);
            std::memcpy(&out[0], &d->maxRadius, 4);
            std::memcpy(&out[4], &d->damage, 4);
            std::memcpy(&out[8], &d->duration, 4);
            break;
        }
        default:
            break;
    }
    return out;
}

inline bool deserializeEntityData(EntityType type, const uint8_t* bytes, size_t len, EntityData& out) {
    switch (type) {
        case EntityType::DroppedItem: {
            if (len < 2) return false;
            DroppedItemData d;
            d.itemType = static_cast<ItemType>(bytes[0]);
            d.count = bytes[1];
            out = d;
            return true;
        }
        case EntityType::Projectile: {
            if (len < 1) return false;
            ProjectileData d;
            d.sourceType = static_cast<ItemType>(bytes[0]);
            out = d;
            return true;
        }
        case EntityType::LaserBeam: {
            if (len < 16) return false;
            LaserBeamData d;
            std::memcpy(&d.endPoint.x, &bytes[0], 4);
            std::memcpy(&d.endPoint.y, &bytes[4], 4);
            std::memcpy(&d.endPoint.z, &bytes[8], 4);
            std::memcpy(&d.damage, &bytes[12], 4);
            out = d;
            return true;
        }
        case EntityType::SaberSwing: {
            if (len < 24) return false;
            SaberSwingData d;
            std::memcpy(&d.forward.x, &bytes[0], 4);
            std::memcpy(&d.forward.y, &bytes[4], 4);
            std::memcpy(&d.forward.z, &bytes[8], 4);
            std::memcpy(&d.range, &bytes[12], 4);
            std::memcpy(&d.damage, &bytes[16], 4);
            std::memcpy(&d.coneCos, &bytes[20], 4);
            out = d;
            return true;
        }
        case EntityType::Explosion: {
            if (len < 12) return false;
            ExplosionData d;
            std::memcpy(&d.maxRadius, &bytes[0], 4);
            std::memcpy(&d.damage, &bytes[4], 4);
            std::memcpy(&d.duration, &bytes[8], 4);
            out = d;
            return true;
        }
        case EntityType::None:
        case EntityType::COUNT:
        default:
            out = std::monostate{};
            return len == 0;
    }
}
