#include "game/host_authority.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>

void HostAuthority::bind(RootState& state, WorldState& worldState, const World* world) {
    m_state = &state;
    m_worldState = &worldState;
    m_world = world;
}

void HostAuthority::setLocalPlayerSync(const std::string& localUUID, LocalPlayerSyncFn fn) {
    m_localUUID = localUUID;
    m_localPlayerSync = std::move(fn);
}

Vec3 HostAuthority::getSpawnPosition() const {
    float worldCenter = (World::WORLD_CHUNKS * World::CHUNK_SIZE) / 2.0f;
    float spawnY = m_world ? m_world->terrainHeightAt(worldCenter, worldCenter) + 2.0f : 2.0f;
    return {worldCenter, spawnY, worldCenter};
}

void HostAuthority::handleDeath(const std::string& uuid) {
    if (!m_state) return;

    auto it = m_state->players.find(uuid);
    if (it == m_state->players.end()) return;
    auto& ps = it->second;

    Vec3 deathPos = ps.position;

    // Drop all inventory items at death position
    for (int i = 0; i < 9; i++) {
        if (ps.inventory[i].type != ItemType::None && ps.inventory[i].count > 0) {
            float angle = (float)i * 0.7f;
            float dx = std::sin(angle) * 0.5f;
            float dz = std::cos(angle) * 0.5f;
            Vec3 dropPos = deathPos + Vec3(dx, 0.5f, dz);
            m_worldState->dropItem(ps.inventory[i].type, ps.inventory[i].count, dropPos,
                                   m_state->world.gameTime, 0);
        }
    }
    ps.clearInventory();
    ps.activeItemType = ItemType::None;
    ps.attackProgress = -1.0f;

    // If local player, sync the Player object's inventory too
    if (uuid == m_localUUID && m_localPlayerSync) {
        m_localPlayerSync(ps);
    }
}

bool HostAuthority::applyDamage(const std::string& victimUUID, float amount, const std::string& attackerUUID) {
    if (!m_state) return false;

    auto vit = m_state->players.find(victimUUID);
    if (vit == m_state->players.end()) return false;
    auto& victim = vit->second;

    if (victim.isDead || amount <= 0.0f) return false;

    victim.health = std::max(0.0f, victim.health - amount);

    if (victim.health <= 0.0f) {
        victim.isDead = true;
        victim.deathTime = m_state->world.gameTime;
        victim.deaths++;

        // Credit the kill to the attacker
        if (!attackerUUID.empty() && attackerUUID != victimUUID) {
            auto ait = m_state->players.find(attackerUUID);
            if (ait != m_state->players.end()) {
                ait->second.kills++;
            }
        }

        handleDeath(victimUUID);
        return true;
    }

    return false;
}

void HostAuthority::respawnPlayer(const std::string& uuid) {
    if (!m_state) return;

    auto it = m_state->players.find(uuid);
    if (it == m_state->players.end()) return;
    auto& ps = it->second;
    if (!ps.isDead) return;

    Vec3 spawnPos = getSpawnPosition();
    ps.position = spawnPos;
    ps.yaw = 0.0f;
    ps.pitch = 0.0f;
    ps.health = 100.0f;
    ps.isDead = false;

    // If this is the local player, also update the Player object and UI
    if (uuid == m_localUUID && m_localPlayerSync) {
        m_localPlayerSync(ps);
    }
}

void HostAuthority::processDeathQueue(const std::vector<std::string>& deathUUIDs) {
    for (const auto& uuid : deathUUIDs) {
        handleDeath(uuid);
    }
}

void HostAuthority::processRespawnQueue(const std::vector<std::string>& respawnUUIDs) {
    for (const auto& uuid : respawnUUIDs) {
        respawnPlayer(uuid);
    }
}

bool HostAuthority::pickupItem(const std::string& playerUUID, uint16_t entityId) {
    if (!m_state || !m_worldState) return false;

    auto pit = m_state->players.find(playerUUID);
    if (pit == m_state->players.end()) return false;
    auto& ps = pit->second;
    if (ps.isDead) return false;

    // Find the entity
    const WorldEntity* entity = m_worldState->findEntity(entityId);
    if (!entity || entity->type != EntityType::DroppedItem) return false;
    const auto* data = std::get_if<DroppedItemData>(&entity->data);
    if (!data) return false;

    // Check if player has room in inventory
    // Build a temporary Inventory from PlayerState to use addItem logic
    Inventory tempInv;
    for (int i = 0; i < 9; i++) {
        if (ps.inventory[i].type != ItemType::None)
            tempInv.getSlot(i) = {ps.inventory[i].type, ps.inventory[i].count};
    }

    if (!tempInv.addItem(data->itemType, data->count)) return false;

    // Commit: update PlayerState inventory from temp
    for (int i = 0; i < 9; i++) {
        const auto& slot = tempInv.getSlot(i);
        ps.inventory[i] = {slot.type, slot.count};
    }

    // Remove entity from world
    m_worldState->removeEntity(entityId);

    // If local player, sync the Player object's inventory
    if (playerUUID == m_localUUID && m_localPlayerSync) {
        m_localPlayerSync(ps);
    }

    return true;
}

bool HostAuthority::useItem(const std::string& playerUUID, int slot) {
    if (!m_state) return false;

    auto pit = m_state->players.find(playerUUID);
    if (pit == m_state->players.end()) return false;
    auto& ps = pit->second;
    if (ps.isDead) return false;
    if (slot < 0 || slot >= 9) return false;

    auto& invSlot = ps.inventory[slot];
    if (invSlot.type == ItemType::None || invSlot.count <= 0) return false;

    invSlot.count--;
    if (invSlot.count <= 0) {
        invSlot = {ItemType::None, 0};
    }

    if (playerUUID == m_localUUID && m_localPlayerSync) {
        m_localPlayerSync(ps);
    }

    return true;
}

void HostAuthority::processEntityDamage() {
    if (!m_state || !m_worldState) return;

    // Build player snapshots using UUIDs directly
    std::vector<PlayerSnapshot> snaps;
    for (const auto& [uuid, ps] : m_state->players) {
        if (ps.isDead) continue;
        if (ps.position.x == 0.0f && ps.position.y == 0.0f && ps.position.z == 0.0f) continue;
        snaps.push_back({uuid, ps.position, 0.3f, 1.8f});
    }

    // Run damage detection on all entities
    for (auto& entity : m_state->entities) {
        std::vector<DamageEvent> events;
        m_worldState->processDamageEntity(entity, snaps, events);
        for (const auto& ev : events) {
            applyDamage(ev.victimUUID, ev.amount, ev.attackerUUID);
        }
    }
}

void HostAuthority::dropItem(const std::string& playerUUID, int slot, const Vec3& dropPos) {
    if (!m_state || !m_worldState) return;

    auto pit = m_state->players.find(playerUUID);
    if (pit == m_state->players.end()) return;
    auto& ps = pit->second;
    if (ps.isDead) return;
    if (slot < 0 || slot >= 9) return;

    auto& invSlot = ps.inventory[slot];
    if (invSlot.type == ItemType::None || invSlot.count <= 0) return;

    // Drop the item into the world
    m_worldState->dropItem(invSlot.type, invSlot.count, dropPos,
                           m_state->world.gameTime, 0);

    // Clear the inventory slot
    invSlot = {ItemType::None, 0};

    // If local player, sync the Player object's inventory
    if (playerUUID == m_localUUID && m_localPlayerSync) {
        m_localPlayerSync(ps);
    }
}
