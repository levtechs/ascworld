#pragma once
#include "game/item.h"
#include <array>

class Inventory {
public:
    static constexpr int HOTBAR_SIZE = 9;

    // --- Query ---
    int selectedSlot() const { return m_selected; }
    const ItemStack& getSlot(int i) const { return m_slots[i]; }
    ItemStack& getSlot(int i) { return m_slots[i]; }
    const ItemStack& activeItem() const { return m_slots[m_selected]; }

    // --- Manipulation ---
    // Add item to first matching stack or empty slot. Returns false if full.
    bool addItem(ItemType type, int count = 1);

    // Remove count items from the active slot (for consumables).
    void removeActiveItem(int count = 1);

    // Drop the active item: removes it and returns what was dropped.
    ItemStack dropActive();

    // --- Cycling ---
    void cycleLeft();
    void cycleRight();
    void selectSlot(int slot);

    // --- Cooldown ---
    void updateCooldown(float dt);
    void startCooldown();
    float getCooldownRemaining() const { return m_cooldownTimer; }
    float getCooldownFraction() const;  // 0.0 = ready, 1.0 = just started
    bool isReady() const { return m_cooldownTimer <= 0.0f; }

private:
    std::array<ItemStack, HOTBAR_SIZE> m_slots{};
    int m_selected = 0;
    float m_cooldownTimer = 0.0f;
    float m_cooldownDuration = 0.0f;  // last cooldown that was started
};
