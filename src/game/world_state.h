#pragma once
#include "game/entity.h"
#include "game/inventory.h"
#include "game/weapon_meshes.h"
#include "core/vec_math.h"
#include <vector>
#include <cstdint>
#include <array>

struct AABB;

struct PlayerSnapshot {
    std::string uuid;
    Vec3 position;
    float radius = 0.3f;
    float height = 1.8f;
};

struct DamageEvent {
    std::string attackerUUID;
    std::string victimUUID;
    float amount = 0.0f;
    EntityType weaponType = EntityType::None;
};

class WorldState {
public:
    WorldState() : m_entities(nullptr) {}
    void bind(std::vector<WorldEntity>& entities);

    uint16_t spawnEntity(const WorldEntity& entity);
    void spawnEntityWithId(const WorldEntity& entity);
    void removeEntity(uint16_t id);
    WorldEntity* findEntity(uint16_t id);
    const WorldEntity* findEntity(uint16_t id) const;

    uint16_t dropItem(ItemType type, int count, const Vec3& pos, float gameTime, uint8_t ownerId = 0);
    bool pickupItem(uint16_t id, Inventory& inv);
    const WorldEntity* findNearestPickable(const Vec3& playerPos, float maxRange = 2.0f) const;

    void update(float dt, float gameTime, const std::vector<AABB>& colliders,
                bool authoritativeDamage, const std::vector<PlayerSnapshot>& players,
                std::vector<DamageEvent>& outDamageEvents);

    void gatherSceneObjects(std::vector<SceneObject>& out, const WeaponMeshes& weaponMeshes,
                            float gameTime) const;

    // Gather beam/effect objects separately (rendered in a second pass so they're not occluded)
    void gatherOverlayObjects(std::vector<SceneObject>& out, const WeaponMeshes& weaponMeshes,
                              float gameTime) const;

    const std::vector<WorldEntity>& entities() const { return *m_entities; }
    void clear();

    // Process damage from a single entity against player snapshots.
    // Public so HostAuthority can run authoritative damage detection.
    void processDamageEntity(WorldEntity& entity, const std::vector<PlayerSnapshot>& players,
                             std::vector<DamageEvent>& outDamageEvents);

private:
    void updateProjectile(WorldEntity& entity, float dt, const std::vector<AABB>& colliders,
                          std::vector<WorldEntity>& spawnQueue);

    std::vector<WorldEntity>* m_entities;
    std::array<uint16_t, 8> m_ownerCounters{};
};
