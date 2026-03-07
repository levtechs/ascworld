#pragma once
#include <cstdint>

// ============================================================
// Item type enum — add new items here
// ============================================================

enum class ItemType : uint8_t {
    None = 0,
    Saber,       // melee weapon
    Laser,       // hitscan ranged weapon
    Flashbang,   // throwable projectile, consumed on use
    // Future: Shotgun, Shield, MedKit, etc.
    COUNT
};

// ============================================================
// Item definition — static properties for each item type
// ============================================================

struct ItemDef {
    ItemType type;
    const char* name;
    const char* shortName;  // 3-char abbreviation for HUD
    float cooldown;         // seconds between uses
    bool consumable;        // destroyed on use?
    int maxStack;           // 1 for weapons, >1 for throwables
    // Future: float damage, float range, float weight, etc.
};

// Global item registry — indexed by (int)ItemType
inline constexpr ItemDef ITEM_DEFS[] = {
    { ItemType::None,      "Empty",     "   ", 0.0f, false, 0 },
    { ItemType::Saber,     "Saber",     "SAB", 0.5f, false, 1 },
    { ItemType::Laser,     "Laser",     "LAS", 0.8f, false, 1 },
    { ItemType::Flashbang, "Flashbang", "FLA", 0.0f, true,  5 },
};

inline const ItemDef& getItemDef(ItemType type) {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(ItemType::COUNT))
        return ITEM_DEFS[0];
    return ITEM_DEFS[idx];
}

// ============================================================
// Item stack — a slot in the inventory
// ============================================================

struct ItemStack {
    ItemType type = ItemType::None;
    int count = 0;

    bool empty() const { return type == ItemType::None || count <= 0; }

    const ItemDef& def() const { return getItemDef(type); }
};
