#include "game/world_state.h"
#include "game/player.h"
#include <algorithm>
#include <cmath>

uint16_t WorldState::spawnEntity(const WorldEntity& entity) {
    WorldEntity e = entity;
    uint8_t owner = static_cast<uint8_t>(e.ownerId & 0x07);
    uint16_t local = ++m_ownerCounters[owner];
    e.id = static_cast<uint16_t>((owner << 13) | (local & 0x1FFF));
    m_entities.push_back(e);
    return e.id;
}

void WorldState::spawnEntityWithId(const WorldEntity& entity) {
    if (entity.id == 0) return;
    for (const auto& e : m_entities) {
        if (e.id == entity.id) return;
    }
    uint8_t owner = static_cast<uint8_t>((entity.id >> 13) & 0x07);
    uint16_t local = static_cast<uint16_t>(entity.id & 0x1FFF);
    if (owner < m_ownerCounters.size() && local > m_ownerCounters[owner]) {
        m_ownerCounters[owner] = local;
    }
    m_entities.push_back(entity);
}

void WorldState::removeEntity(uint16_t id) {
    m_entities.erase(
        std::remove_if(m_entities.begin(), m_entities.end(),
            [id](const WorldEntity& e) { return e.id == id; }),
        m_entities.end());
}

WorldEntity* WorldState::findEntity(uint16_t id) {
    for (auto& e : m_entities) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

const WorldEntity* WorldState::findEntity(uint16_t id) const {
    for (const auto& e : m_entities) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

uint16_t WorldState::dropItem(ItemType type, int count, const Vec3& pos, float gameTime, uint8_t ownerId) {
    WorldEntity e;
    e.type = EntityType::DroppedItem;
    e.ownerId = ownerId;
    e.position = pos;
    e.spawnTime = gameTime;
    DroppedItemData data;
    data.itemType = type;
    data.count = static_cast<uint8_t>(std::max(1, std::min(count, 255)));
    e.data = data;
    return spawnEntity(e);
}

bool WorldState::pickupItem(uint16_t id, Inventory& inv) {
    for (auto it = m_entities.begin(); it != m_entities.end(); ++it) {
        if (it->id != id || it->type != EntityType::DroppedItem) continue;
        const auto* data = std::get_if<DroppedItemData>(&it->data);
        if (!data) return false;
        if (inv.addItem(data->itemType, data->count)) {
            m_entities.erase(it);
            return true;
        }
        return false;
    }
    return false;
}

const WorldEntity* WorldState::findNearestPickable(const Vec3& playerPos, float maxRange) const {
    const WorldEntity* best = nullptr;
    float bestDist = maxRange * maxRange;
    for (const auto& e : m_entities) {
        if (e.type != EntityType::DroppedItem) continue;
        float dx = e.position.x - playerPos.x;
        float dy = e.position.y - playerPos.y;
        float dz = e.position.z - playerPos.z;
        float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < bestDist) {
            bestDist = d2;
            best = &e;
        }
    }
    return best;
}

void WorldState::updateProjectile(WorldEntity& entity, float dt,
                                  const std::vector<AABB>& colliders,
                                  std::vector<WorldEntity>& spawnQueue) {
    entity.velocity.y -= 15.0f * dt;
    entity.position = entity.position + entity.velocity * dt;

    bool collided = false;
    if (entity.position.y <= 0.0f) {
        entity.position.y = 0.0f;
        collided = true;
    }

    if (!collided) {
        for (const auto& box : colliders) {
            if (entity.position.x >= box.min.x && entity.position.x <= box.max.x &&
                entity.position.y >= box.min.y && entity.position.y <= box.max.y &&
                entity.position.z >= box.min.z && entity.position.z <= box.max.z) {
                collided = true;
                break;
            }
        }
    }

    if (collided) {
        WorldEntity ex;
        ex.type = EntityType::Explosion;
        ex.ownerId = entity.ownerId;
        ex.position = entity.position;
        ex.lifetime = 0.35f;
        ExplosionData data;
        data.maxRadius = 3.8f;
        data.damage = 42.0f;
        data.duration = 0.35f;
        ex.data = data;
        spawnQueue.push_back(ex);
        entity.lifetime = 0.0f;
    }
}

void WorldState::processDamageEntity(WorldEntity& entity,
                                     const std::vector<PlayerSnapshot>& players,
                                     std::vector<DamageEvent>& outDamageEvents) {
    switch (entity.type) {
        case EntityType::LaserBeam: {
            const auto* data = std::get_if<LaserBeamData>(&entity.data);
            if (!data) return;

            Vec3 start = entity.position;
            Vec3 end = data->endPoint;
            Vec3 seg = end - start;
            float segLenSq = seg.dot(seg);
            if (segLenSq < 1e-4f) return;

            for (const auto& p : players) {
                if (p.peerId == entity.ownerId) continue;
                if ((entity.hitMask & (1u << p.peerId)) != 0) continue;

                Vec3 toPlayer = p.position + Vec3(0.0f, p.height * 0.5f, 0.0f) - start;
                float t = toPlayer.dot(seg) / segLenSq;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                Vec3 closest = start + seg * t;

                Vec3 center = p.position + Vec3(0.0f, p.height * 0.5f, 0.0f);
                float d = (center - closest).length();
                if (d <= p.radius + 0.35f) {
                    DamageEvent ev;
                    ev.sourceId = entity.ownerId;
                    ev.targetId = p.peerId;
                    ev.amount = data->damage;
                    ev.weaponType = EntityType::LaserBeam;
                    outDamageEvents.push_back(ev);
                    entity.hitMask |= static_cast<uint8_t>(1u << p.peerId);
                }
            }
            break;
        }

        case EntityType::SaberSwing: {
            const auto* data = std::get_if<SaberSwingData>(&entity.data);
            if (!data) return;

            Vec3 fwd = data->forward.normalized();
            for (const auto& p : players) {
                if (p.peerId == entity.ownerId) continue;
                if ((entity.hitMask & (1u << p.peerId)) != 0) continue;

                Vec3 target = p.position + Vec3(0.0f, p.height * 0.5f, 0.0f);
                Vec3 to = target - entity.position;
                float dist = to.length();
                if (dist > data->range) continue;

                Vec3 dir = (dist > 0.001f) ? (to * (1.0f / dist)) : Vec3(0.0f, 0.0f, 0.0f);
                float cosA = fwd.dot(dir);
                if (cosA < data->coneCos) continue;

                DamageEvent ev;
                ev.sourceId = entity.ownerId;
                ev.targetId = p.peerId;
                ev.amount = data->damage;
                ev.weaponType = EntityType::SaberSwing;
                outDamageEvents.push_back(ev);
                entity.hitMask |= static_cast<uint8_t>(1u << p.peerId);
            }
            break;
        }

        case EntityType::Explosion: {
            const auto* data = std::get_if<ExplosionData>(&entity.data);
            if (!data || data->duration <= 0.001f) return;

            float age = data->duration - entity.lifetime;
            if (age < 0.0f) age = 0.0f;
            if (age > data->duration) age = data->duration;
            float t = age / data->duration;
            float radius = data->maxRadius * t;

            for (const auto& p : players) {
                if (p.peerId == entity.ownerId) continue;
                if ((entity.hitMask & (1u << p.peerId)) != 0) continue;

                Vec3 center = p.position + Vec3(0.0f, p.height * 0.5f, 0.0f);
                float d = (center - entity.position).length();
                if (d > radius + p.radius) continue;

                float falloff = 1.0f - std::min(1.0f, d / std::max(0.001f, data->maxRadius));
                DamageEvent ev;
                ev.sourceId = entity.ownerId;
                ev.targetId = p.peerId;
                ev.amount = data->damage * (0.35f + 0.65f * falloff);
                ev.weaponType = EntityType::Explosion;
                outDamageEvents.push_back(ev);
                entity.hitMask |= static_cast<uint8_t>(1u << p.peerId);
            }
            break;
        }

        default:
            break;
    }
}

void WorldState::update(float dt, float gameTime, const std::vector<AABB>& colliders,
                        bool authoritativeDamage, const std::vector<PlayerSnapshot>& players,
                        std::vector<DamageEvent>& outDamageEvents) {
    std::vector<WorldEntity> spawnQueue;

    for (auto& e : m_entities) {
        if (e.lifetime >= 0.0f) {
            e.lifetime -= dt;
            if (e.lifetime <= 0.0f) e.lifetime = 0.0f;
        }

        if (e.type == EntityType::Projectile && e.lifetime > 0.0f) {
            updateProjectile(e, dt, colliders, spawnQueue);
        }

        if (authoritativeDamage) {
            processDamageEntity(e, players, outDamageEvents);
        }
    }

    for (auto& e : spawnQueue) {
        e.spawnTime = gameTime;
        spawnEntity(e);
    }

    m_entities.erase(
        std::remove_if(m_entities.begin(), m_entities.end(),
            [](const WorldEntity& e) { return e.lifetime == 0.0f; }),
        m_entities.end());
}

void WorldState::gatherSceneObjects(std::vector<SceneObject>& out,
                                    const WeaponMeshes& weaponMeshes,
                                    float gameTime) const {
    static Mesh explosionMesh = createSphere(7, 10, 1.0f);
    static bool init = false;
    if (!init) {
        explosionMesh.computeBoundingRadius();
        init = true;
    }

    for (const auto& e : m_entities) {
        switch (e.type) {
            case EntityType::DroppedItem: {
                const auto* data = std::get_if<DroppedItemData>(&e.data);
                if (!data) break;
                float elapsed = gameTime - e.spawnTime;
                auto objs = weaponMeshes.getDroppedObjects(data->itemType, e.position, elapsed);
                for (auto& obj : objs) out.push_back(std::move(obj));
                break;
            }

            case EntityType::Projectile: {
                const auto* data = std::get_if<ProjectileData>(&e.data);
                if (!data) break;
                auto objs = weaponMeshes.getDroppedObjects(data->sourceType, e.position, gameTime);
                for (auto& obj : objs) out.push_back(std::move(obj));
                break;
            }

            case EntityType::LaserBeam:
                // Rendered separately via gatherOverlayObjects() to avoid occlusion
                break;

            case EntityType::Explosion: {
                const auto* data = std::get_if<ExplosionData>(&e.data);
                if (!data || data->duration <= 0.001f) break;
                float age = data->duration - e.lifetime;
                float t = std::min(1.0f, std::max(0.0f, age / data->duration));
                float r = std::max(0.05f, data->maxRadius * t);
                float glow = 1.2f - t;
                Material m(Color3(2.6f * glow, 1.6f * glow, 0.35f * glow), 16.0f, 0.1f);
                SceneObject so;
                so.mesh = &explosionMesh;
                so.transform = Mat4::translate(e.position) * Mat4::scale(Vec3(r, r, r));
                so.material = m;
                out.push_back(so);
                break;
            }

            default:
                break;
        }
    }
}

void WorldState::gatherOverlayObjects(std::vector<SceneObject>& out,
                                      const WeaponMeshes& weaponMeshes,
                                      float gameTime) const {
    (void)gameTime;
    for (const auto& e : m_entities) {
        if (e.type == EntityType::LaserBeam) {
            const auto* data = std::get_if<LaserBeamData>(&e.data);
            if (!data) continue;
            auto objs = weaponMeshes.getLaserBeamObjects(e.position, data->endPoint);
            for (auto& obj : objs) out.push_back(std::move(obj));
        }
    }
}

void WorldState::clear() {
    m_entities.clear();
    m_ownerCounters.fill(0);
}
