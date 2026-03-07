#include "game/player.h"
#include "world/world.h"
#include <cmath>
#include <algorithm>

// --- SlopeCollider ---

bool SlopeCollider::containsXZ(float x, float z) const {
    return x >= baseMin.x && x <= baseMax.x && z >= baseMin.z && z <= baseMax.z;
}

float SlopeCollider::heightAt(float x, float z) const {
    float t;
    if (rampAxis == 0) {
        t = (x - baseMin.x) / (baseMax.x - baseMin.x);
    } else {
        t = (z - baseMin.z) / (baseMax.z - baseMin.z);
    }
    if (!rampPositive) t = 1.0f - t;
    t = std::max(0.0f, std::min(1.0f, t));
    return heightAtStart + t * (heightAtEnd - heightAtStart);
}

Vec3 SlopeCollider::surfaceNormal() const {
    // The slope surface goes from heightAtStart to heightAtEnd along the ramp axis.
    // We can compute the normal from two tangent vectors on the surface.
    // Tangent along ramp axis: direction of increasing height
    float rise = heightAtEnd - heightAtStart;
    float run;
    Vec3 tangentAlongRamp;
    if (rampAxis == 0) {
        run = baseMax.x - baseMin.x;
        float dir = rampPositive ? 1.0f : -1.0f;
        tangentAlongRamp = Vec3(dir * run, rise, 0.0f);
    } else {
        run = baseMax.z - baseMin.z;
        float dir = rampPositive ? 1.0f : -1.0f;
        tangentAlongRamp = Vec3(0.0f, rise, dir * run);
    }
    // Tangent across the ramp (horizontal, perpendicular to ramp direction)
    Vec3 tangentAcross;
    if (rampAxis == 0) {
        tangentAcross = Vec3(0.0f, 0.0f, 1.0f);
    } else {
        tangentAcross = Vec3(1.0f, 0.0f, 0.0f);
    }
    // Normal = cross(tangentAcross, tangentAlongRamp) for upward-pointing normal
    Vec3 n = tangentAcross.cross(tangentAlongRamp).normalized();
    // Ensure it points upward
    if (n.y < 0.0f) n = n * -1.0f;
    return n;
}

// --- Player ---

Player::Player()
    : m_pos(0.0f, 0.0f, 5.0f),
      m_velocity(0.0f, 0.0f, 0.0f),
      m_yaw(0.0f),
      m_pitch(0.0f)
{
    m_mesh = createCylinder(m_radius, m_height, 12);

    Material playerMat(Color3(0.3f, 0.5f, 0.8f), 16.0f, 0.3f);

    m_sceneObj.mesh = &m_mesh;
    m_sceneObj.material = playerMat;
    updateTransform();
}

void Player::setTerrainQuery(const World* world) {
    m_world = world;
}

float Player::getGroundHeight() const {
    if (m_world) {
        return m_world->terrainHeightAt(m_pos.x, m_pos.z);
    }
    return 0.0f; // Flat ground at Y=0
}

Vec3 Player::getGroundNormal() const {
    if (m_world) {
        return m_world->terrainNormalAt(m_pos.x, m_pos.z);
    }
    return Vec3(0.0f, 1.0f, 0.0f);
}

Vec3 Player::eyePosition() const {
    return Vec3(m_pos.x, m_pos.y + m_eyeOffset, m_pos.z);
}

Vec3 Player::forward() const {
    return Vec3(
        std::sin(m_yaw) * std::cos(m_pitch),
        std::sin(m_pitch),
        -std::cos(m_yaw) * std::cos(m_pitch)
    );
}

Vec3 Player::right() const {
    return Vec3(std::cos(m_yaw), 0.0f, std::sin(m_yaw));
}

// --- Contact detection ---

bool Player::checkGroundContact(Contact& contact) const {
    // Use terrain height instead of hardcoded Y=0
    float groundY = getGroundHeight();
    float feetY = m_pos.y;

    if (feetY < groundY) {
        Vec3 normal = getGroundNormal();
        contact.normal = normal;
        contact.penetration = groundY - feetY;
        return true;
    }
    // Also detect "resting on ground" when very close
    if (feetY < groundY + 0.05f && m_velocity.y <= 0.0f) {
        Vec3 normal = getGroundNormal();
        contact.normal = normal;
        contact.penetration = groundY + 0.05f - feetY;
        return true;
    }
    return false;
}

bool Player::checkAABBContact(const AABB& box, Contact& contact) const {
    // Player is a cylinder: circle of m_radius in XZ, vertical extent [m_pos.y, m_pos.y + m_height]
    float playerBottom = m_pos.y;
    float playerTop = m_pos.y + m_height;

    // First check Y overlap
    if (playerTop <= box.min.y || playerBottom >= box.max.y) return false;

    // Find closest point on AABB to player axis in XZ
    float closestX = std::max(box.min.x, std::min(m_pos.x, box.max.x));
    float closestZ = std::max(box.min.z, std::min(m_pos.z, box.max.z));

    float dx = m_pos.x - closestX;
    float dz = m_pos.z - closestZ;
    float distXZ = std::sqrt(dx * dx + dz * dz);

    // Check if player is inside the XZ footprint of the box
    bool insideXZ = (m_pos.x >= box.min.x && m_pos.x <= box.max.x &&
                     m_pos.z >= box.min.z && m_pos.z <= box.max.z);

    if (!insideXZ && distXZ >= m_radius) return false;

    // We have a collision. Determine the best push-out direction.
    // Compute penetration for each possible push direction.

    float penTop = playerTop - box.min.y;    // push player down (or box up)
    float penBottom = box.max.y - playerBottom; // push player up (stand on top)
    float penXZ;
    Vec3 xzNormal;

    if (insideXZ) {
        // Player center is inside box XZ — find shortest XZ push
        float pushMinX = m_pos.x - box.min.x + m_radius;
        float pushMaxX = box.max.x - m_pos.x + m_radius;
        float pushMinZ = m_pos.z - box.min.z + m_radius;
        float pushMaxZ = box.max.z - m_pos.z + m_radius;
        float minPush = std::min({pushMinX, pushMaxX, pushMinZ, pushMaxZ});
        penXZ = minPush;
        if (minPush == pushMinX)      xzNormal = Vec3(-1, 0, 0);
        else if (minPush == pushMaxX) xzNormal = Vec3(1, 0, 0);
        else if (minPush == pushMinZ) xzNormal = Vec3(0, 0, -1);
        else                          xzNormal = Vec3(0, 0, 1);
    } else {
        penXZ = m_radius - distXZ;
        xzNormal = Vec3(dx / distXZ, 0.0f, dz / distXZ);
    }

    // Choose the axis with minimal penetration (least displacement to resolve)
    // But prefer pushing up (standing on top) if the player is mostly above the box
    float playerMidY = playerBottom + m_height * 0.5f;
    float boxMidY = (box.min.y + box.max.y) * 0.5f;

    // If player feet are near the top of the box, prefer vertical push-up (stand on it)
    if (playerBottom >= box.max.y - 0.5f && penBottom < penXZ) {
        contact.normal = Vec3(0, 1, 0);
        contact.penetration = penBottom;
        return true;
    }

    // If player head is near the bottom of the box, push down
    if (playerTop <= box.min.y + 0.5f && penTop < penXZ) {
        contact.normal = Vec3(0, -1, 0);
        contact.penetration = penTop;
        return true;
    }

    // Otherwise use XZ push (wall collision)
    if (penXZ > 0.0f) {
        contact.normal = xzNormal;
        contact.penetration = penXZ;
        return true;
    }

    return false;
}

bool Player::checkSlopeContact(const SlopeCollider& slope, Contact& contact) const {
    if (!slope.containsXZ(m_pos.x, m_pos.z)) return false;

    float surfaceY = slope.heightAt(m_pos.x, m_pos.z);
    float feetY = m_pos.y;

    // Only interact if player feet are at or below the slope surface
    if (feetY > surfaceY + 0.01f) return false;

    float penetration = surfaceY - feetY;
    if (penetration < -0.01f) return false; // player is well above

    contact.normal = slope.surfaceNormal();
    contact.penetration = std::max(0.0f, penetration) + 0.01f;
    return true;
}

void Player::gatherContacts(const std::vector<AABB>& colliders,
                            const std::vector<SlopeCollider>& slopes,
                            std::vector<Contact>& contacts) const {
    Contact c;

    // Ground (terrain heightmap or flat Y=0)
    if (checkGroundContact(c)) {
        contacts.push_back(c);
    }

    // AABB colliders
    for (const auto& box : colliders) {
        if (checkAABBContact(box, c)) {
            contacts.push_back(c);
        }
    }

    // Slopes
    for (const auto& slope : slopes) {
        if (checkSlopeContact(slope, c)) {
            contacts.push_back(c);
        }
    }
}

// --- Update ---

void Player::update(const InputState& input, float dt,
                    const std::vector<AABB>& colliders,
                    const std::vector<SlopeCollider>& slopes) {
    // Mouse look
    if (input.mouseDx != 0.0f || input.mouseDy != 0.0f) {
        m_yaw   += input.mouseDx * mouseSensitivity;
        m_pitch -= input.mouseDy * mouseSensitivity;
        m_pitch = std::max(-1.4f, std::min(1.4f, m_pitch));
    }

    // Crouch
    bool wantCrouch = input.down.load(std::memory_order_relaxed);
    float targetEyeOffset = wantCrouch ? m_crouchEyeOffset : m_standingEyeOffset;
    float crouchSpeed = 8.0f;
    if (m_eyeOffset < targetEyeOffset) {
        m_eyeOffset = std::min(m_eyeOffset + crouchSpeed * dt, targetEyeOffset);
    } else if (m_eyeOffset > targetEyeOffset) {
        m_eyeOffset = std::max(m_eyeOffset - crouchSpeed * dt, targetEyeOffset);
    }
    m_crouching = wantCrouch;

    // === Horizontal movement input ===
    Vec3 fwd = Vec3(std::sin(m_yaw), 0.0f, -std::cos(m_yaw)).normalized();
    Vec3 rgt = Vec3(std::cos(m_yaw), 0.0f, std::sin(m_yaw)).normalized();

    Vec3 wishDir(0, 0, 0);
    if (input.forward.load(std::memory_order_relaxed))  wishDir += fwd;
    if (input.backward.load(std::memory_order_relaxed)) wishDir -= fwd;
    if (input.right.load(std::memory_order_relaxed))    wishDir += rgt;
    if (input.left.load(std::memory_order_relaxed))     wishDir -= rgt;

    float speed = moveSpeed * (m_crouching ? 0.5f : 1.0f);

    // Set horizontal velocity directly from input (responsive FPS feel)
    if (wishDir.length() > 0.001f) {
        Vec3 moveVel = wishDir.normalized() * speed;
        m_velocity.x = moveVel.x;
        m_velocity.z = moveVel.z;
    } else {
        // Friction / stop immediately when no input
        m_velocity.x = 0.0f;
        m_velocity.z = 0.0f;
    }

    // === Gravity ===
    const float gravity = 15.0f; // m/s^2
    m_velocity.y -= gravity * dt;

    // === Jump ===
    if (input.up.load(std::memory_order_relaxed) && m_onGround) {
        m_velocity.y = 5.5f;
        m_onGround = false;
    }

    // === Integrate position ===
    m_pos += m_velocity * dt;

    // === Resolve contacts (multiple iterations for stability) ===
    m_onGround = false;
    const int maxIterations = 4;

    for (int iter = 0; iter < maxIterations; iter++) {
        std::vector<Contact> contacts;
        gatherContacts(colliders, slopes, contacts);

        if (contacts.empty()) break;

        for (const auto& c : contacts) {
            // Push player out of geometry along contact normal
            m_pos += c.normal * c.penetration;

            // Cancel velocity component going into the surface
            float velIntoSurface = m_velocity.dot(c.normal);
            if (velIntoSurface < 0.0f) {
                m_velocity -= c.normal * velIntoSurface;
            }

            // Determine if this contact is "ground" (walkable)
            if (c.normal.y >= minGroundNormalY) {
                m_onGround = true;
            }

            // If surface is too steep (not walkable), apply sliding:
            // remove the horizontal component that would make us stick to the wall,
            // but let gravity pull us down naturally (already handled by not granting onGround)
            if (c.normal.y > 0.1f && c.normal.y < minGroundNormalY) {
                // Steep slope: project gravity-induced velocity onto the slope surface
                // so the player slides downhill. The velocity cancellation above already
                // prevents moving into the surface. Gravity does the rest.
            }
        }
    }

    // Clamp: never go below terrain ground (safety net)
    float groundY = getGroundHeight();
    if (m_pos.y < groundY) {
        m_pos.y = groundY;
        if (m_velocity.y < 0.0f) m_velocity.y = 0.0f;
        m_onGround = true;
    }

    updateTransform();
}

void Player::updateTransform() {
    m_sceneObj.transform = Mat4::translate(m_pos);
}
