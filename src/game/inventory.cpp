#include "game/inventory.h"
#include <algorithm>

bool Inventory::addItem(ItemType type, int count) {
    if (type == ItemType::None || count <= 0) return false;
    const auto& def = getItemDef(type);

    // First try to stack onto existing matching slot
    if (def.maxStack > 1) {
        for (auto& slot : m_slots) {
            if (slot.type == type && slot.count < def.maxStack) {
                int space = def.maxStack - slot.count;
                int add = std::min(count, space);
                slot.count += add;
                count -= add;
                if (count <= 0) return true;
            }
        }
    }

    // Then try empty slots
    for (auto& slot : m_slots) {
        if (slot.empty()) {
            int add = std::min(count, def.maxStack);
            slot.type = type;
            slot.count = add;
            count -= add;
            if (count <= 0) return true;
        }
    }

    return count <= 0; // false if items remain
}

void Inventory::removeActiveItem(int count) {
    auto& slot = m_slots[m_selected];
    if (slot.empty()) return;
    slot.count -= count;
    if (slot.count <= 0) {
        slot.type = ItemType::None;
        slot.count = 0;
    }
}

ItemStack Inventory::dropActive() {
    ItemStack dropped = m_slots[m_selected];
    m_slots[m_selected] = {};
    return dropped;
}

void Inventory::cycleLeft() {
    // Cycle to previous non-empty slot, or wrap around
    for (int i = 1; i <= HOTBAR_SIZE; i++) {
        int idx = (m_selected - i + HOTBAR_SIZE) % HOTBAR_SIZE;
        m_selected = idx;
        return;
    }
}

void Inventory::cycleRight() {
    for (int i = 1; i <= HOTBAR_SIZE; i++) {
        int idx = (m_selected + i) % HOTBAR_SIZE;
        m_selected = idx;
        return;
    }
}

void Inventory::selectSlot(int slot) {
    if (slot >= 0 && slot < HOTBAR_SIZE) {
        m_selected = slot;
    }
}

void Inventory::updateCooldown(float dt) {
    if (m_cooldownTimer > 0.0f) {
        m_cooldownTimer -= dt;
        if (m_cooldownTimer < 0.0f) m_cooldownTimer = 0.0f;
    }
}

void Inventory::startCooldown() {
    const auto& item = activeItem();
    if (!item.empty()) {
        m_cooldownDuration = item.def().cooldown;
        m_cooldownTimer = m_cooldownDuration;
    }
}

float Inventory::getCooldownFraction() const {
    if (m_cooldownDuration <= 0.0f) return 0.0f;
    return m_cooldownTimer / m_cooldownDuration;
}
