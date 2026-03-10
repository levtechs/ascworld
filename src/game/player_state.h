#pragma once
#include "core/vec_math.h"
#include "game/item.h"
#include "game/character_appearance.h"
#include <string>
#include <array>
#include <cstdint>
#include <unordered_map>

// A single inventory slot for PlayerState (network-friendly POD)
struct ItemSlot {
    ItemType type = ItemType::None;
    int count = 0;
    bool empty() const { return type == ItemType::None || count <= 0; }
};

// Canonical representation of a player's game state.
// One per player (local or remote), stored in a single map keyed by UUID.
struct PlayerState {
    // Identity (immutable for session)
    std::string uuid;           // persistent unique ID
    std::string name;           // display name
    uint8_t peerId = 255;       // current session-local alias (wire format only)

    // Authoritative game state (host controls health)
    Vec3 position;
    float yaw = 0, pitch = 0;
    float health = 100;
    int kills = 0, deaths = 0;
    bool isDead = false;
    float deathTime = 0;        // game time when death occurred (for fall animation)

    // Inventory
    std::array<ItemSlot, 9> inventory{};
    int selectedSlot = 0;

    // Visual
    CharacterAppearance appearance;

    // Combat state (synced for remote player rendering)
    ItemType activeItemType = ItemType::None;
    float attackProgress = -1.0f; // -1 = idle, 0..1 = attacking

    // Timestamps
    float lastUpdateTime = 0;   // for stale detection
    float gameTime = 0;         // game time at last sync

    // Set default loadout (Saber, Laser, Flashbang x3)
    void setDefaultLoadout() {
        for (auto& s : inventory) s = {};
        inventory[0] = {ItemType::Saber, 1};
        inventory[1] = {ItemType::Laser, 1};
        inventory[2] = {ItemType::Flashbang, 3};
        selectedSlot = 0;
    }

    // Clear inventory
    void clearInventory() {
        for (auto& s : inventory) s = {};
        selectedSlot = 0;
    }

    // Reset to spawn defaults
    void resetToDefaults(const Vec3& spawnPos) {
        position = spawnPos;
        yaw = 0;
        pitch = 0;
        health = 100;
        kills = 0;
        deaths = 0;
        isDead = false;
        setDefaultLoadout();
        gameTime = 0;
    }
};
