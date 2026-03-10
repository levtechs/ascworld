#include "game/combat.h"
#include "game/player.h"
#include "game/inventory.h"
#include <cmath>

float CombatSystem::attackProgress() const {
    if (!m_attacking || m_attackDuration <= 0.0f) return 0.0f;
    return 1.0f - (m_attackTimer / m_attackDuration);
}

std::vector<WorldEntity> CombatSystem::update(Player& player, bool attackPressed, float dt, const std::string& ownerUUID) {
    std::vector<WorldEntity> spawned;
    Inventory& inv = player.inventory();

    // Tick cooldown
    inv.updateCooldown(dt);

    // Tick attack animation
    if (m_attacking) {
        m_attackTimer -= dt;
        if (m_attackTimer <= 0.0f) {
            m_attacking = false;
            m_attackTimer = 0.0f;
        }
    }

    // Handle attack input
    if (attackPressed && !m_attacking && inv.isReady()) {
        const ItemStack& active = inv.activeItem();
        if (!active.empty()) {
            switch (active.type) {
                case ItemType::Saber:
                    attackSaber(player, spawned, ownerUUID);
                    break;
                case ItemType::Laser:
                    attackLaser(player, spawned, ownerUUID);
                    break;
                case ItemType::Flashbang:
                    attackFlashbang(player, spawned, ownerUUID);
                    break;
                default:
                    break;
            }
        }
    }

    return spawned;
}

void CombatSystem::attackSaber(Player& player, std::vector<WorldEntity>& out, const std::string& ownerUUID) {
    Inventory& inv = player.inventory();

    // Start swing animation
    m_attacking = true;
    m_attackType = ItemType::Saber;
    m_attackDuration = 0.3f;
    m_attackTimer = m_attackDuration;

    // Start cooldown
    inv.startCooldown();

    // Spawn transient melee hit entity for world-state damage handling
    WorldEntity e;
    e.type = EntityType::SaberSwing;
    e.ownerUUID = ownerUUID;
    e.position = player.eyePosition();
    e.lifetime = 0.18f;
    SaberSwingData data;
    data.forward = player.forward();
    data.range = 2.2f;
    data.damage = 30.0f;
    data.coneCos = 0.40f;
    e.data = data;
    out.push_back(e);
}

void CombatSystem::attackLaser(Player& player, std::vector<WorldEntity>& out, const std::string& ownerUUID) {
    Inventory& inv = player.inventory();

    // Start brief recoil animation
    m_attacking = true;
    m_attackType = ItemType::Laser;
    m_attackDuration = 0.15f;
    m_attackTimer = m_attackDuration;

    // Start cooldown
    inv.startCooldown();

    Vec3 eye = player.eyePosition();
    Vec3 fwd = player.forward();
    float beamLength = 80.0f;

    // Compute right vector for weapon muzzle offset
    float cy = std::cos(player.yaw()), sy = std::sin(player.yaw());
    Vec3 right(cy, 0.0f, sy);

    // Start beam from weapon muzzle position (offset right and slightly down)
    // so the player can see the beam line, not just its cross-section
    Vec3 muzzle = eye + right * 0.22f + Vec3(0.0f, -0.15f, 0.0f) + fwd * 0.6f;

    WorldEntity e;
    e.type = EntityType::LaserBeam;
    e.ownerUUID = ownerUUID;
    e.position = muzzle;
    e.lifetime = 0.4f;  // visible for ~12 frames at 30fps, enough for network sync
    LaserBeamData data;
    data.endPoint = muzzle + fwd * beamLength;
    data.damage = 18.0f;
    e.data = data;
    out.push_back(e);
}

void CombatSystem::attackFlashbang(Player& player, std::vector<WorldEntity>& out, const std::string& ownerUUID) {
    Inventory& inv = player.inventory();

    // Must have at least one
    if (inv.activeItem().count <= 0) return;

    // Start throw animation
    m_attacking = true;
    m_attackType = ItemType::Flashbang;
    m_attackDuration = 0.25f;
    m_attackTimer = m_attackDuration;

    // Note: consumable decrement is handled by HostAuthority (via useItem),
    // not here. The game session calls useItem when it sees a flashbang spawn.

    WorldEntity e;
    e.type = EntityType::Projectile;
    e.ownerUUID = ownerUUID;
    e.position = player.eyePosition() + player.forward() * 0.7f;
    e.velocity = player.forward() * 15.0f + Vec3{0.0f, 3.0f, 0.0f};
    e.lifetime = 3.0f;
    ProjectileData data;
    data.sourceType = ItemType::Flashbang;
    e.data = data;
    out.push_back(e);

    // No cooldown for flashbang (consumable) — the throw animation prevents spam
}
