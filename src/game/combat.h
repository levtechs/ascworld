#pragma once
#include "game/entity.h"
#include "game/item.h"
#include "core/vec_math.h"
#include <cstdint>
#include <vector>

class Player;

// Manages attack actions and cooldowns.
// Dynamic entities (projectiles, beams, explosions) are emitted as events.

class CombatSystem {
public:
    CombatSystem() = default;

    // Call once per frame. Handles attack input and animation timing.
    // Returns spawned world entities for this frame (0 or 1 currently).
    std::vector<WorldEntity> update(Player& player, bool attackPressed, float dt, uint8_t ownerId);

    // --- Attack state (for rendering) ---
    bool isAttacking() const { return m_attacking; }
    float attackProgress() const;  // 0..1 over the attack duration
    ItemType attackingWith() const { return m_attackType; }

private:
    void attackSaber(Player& player, std::vector<WorldEntity>& out, uint8_t ownerId);
    void attackLaser(Player& player, std::vector<WorldEntity>& out, uint8_t ownerId);
    void attackFlashbang(Player& player, std::vector<WorldEntity>& out, uint8_t ownerId);

    // Attack animation state
    bool m_attacking = false;
    float m_attackTimer = 0.0f;
    float m_attackDuration = 0.0f;
    ItemType m_attackType = ItemType::None;
};
