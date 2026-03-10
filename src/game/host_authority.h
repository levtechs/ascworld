#pragma once
#include "game/root_state.h"
#include "game/world_state.h"
#include "game/player.h"
#include "game/combat.h"
#include "game/inventory.h"
#include <string>
#include <functional>

class World;

// HostAuthority encapsulates all host-authoritative game logic.
// It runs on the same thread as the game loop (synchronous loopback).
// The game loop never checks if it's the host - instead, NetworkingManager
// routes requests here (if local host) or over the network (if remote host).
//
// This class manages:
//   - Damage application (health reduction, death detection, kill counting)
//   - Death handling (drop inventory items)
//   - Respawn logic (reset position/health)
//   - Processing queued death/respawn events from remote clients

class HostAuthority {
public:
    HostAuthority() = default;

    // Bind to the shared game state. Must be called before any other methods.
    // worldState: for item drops. world: for spawn position queries.
    void bind(RootState& state, WorldState& worldState, const World* world);

    // Apply damage to a player. Handles health reduction, death detection,
    // kill attribution, and item drops on death.
    // Returns true if the victim died from this damage.
    bool applyDamage(const std::string& victimUUID, float amount, const std::string& attackerUUID);

    // Respawn a dead player at the world spawn point.
    // localUUID: if this matches, the callback is invoked to sync the local Player object.
    void respawnPlayer(const std::string& uuid);

    // Process a death event (drop items, clear inventory).
    // Called when a player dies - either from applyDamage or from a remote DamageReport.
    void handleDeath(const std::string& uuid);

    // Set callback for when the local player needs to be synced after a host-side change.
    // (e.g., after respawn, the local Player object's position/inventory need updating)
    using LocalPlayerSyncFn = std::function<void(const PlayerState& ps)>;
    void setLocalPlayerSync(const std::string& localUUID, LocalPlayerSyncFn fn);

    // Process queued death events and respawn requests from remote clients.
    // Called by NetworkingManager during update.
    void processDeathQueue(const std::vector<std::string>& deathUUIDs);
    void processRespawnQueue(const std::vector<std::string>& respawnUUIDs);

    // Item pickup: remove entity from world, add to player's inventory.
    // Returns true if successful.
    bool pickupItem(const std::string& playerUUID, uint16_t entityId);

    // Item drop: remove from player's inventory, spawn dropped entity in world.
    // slot: inventory slot to drop from. dropPos: world position for the drop.
    void dropItem(const std::string& playerUUID, int slot, const Vec3& dropPos);

    // Use/consume an item from a player's inventory slot.
    // Decrements count by 1. Removes the slot if count reaches 0.
    // Returns true if the item was consumed.
    bool useItem(const std::string& playerUUID, int slot);

    // Get spawn position for the current world
    Vec3 getSpawnPosition() const;

private:
    RootState* m_state = nullptr;
    WorldState* m_worldState = nullptr;
    const World* m_world = nullptr;

    std::string m_localUUID;
    LocalPlayerSyncFn m_localPlayerSync;
};
