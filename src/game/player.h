#pragma once
#include "core/vec_math.h"
#include "rendering/mesh.h"
#include "input/input.h"
#include "game/inventory.h"
#include <vector>

// Axis-aligned bounding box for collision
struct AABB {
    Vec3 min, max;
    AABB() {}
    AABB(const Vec3& mn, const Vec3& mx) : min(mn), max(mx) {}
};

// Slope collider: a ramp defined by a base AABB region and a surface plane
struct SlopeCollider {
    Vec3 baseMin, baseMax;   // XZ footprint + Y range
    float heightAtStart;     // Y at the low end of the ramp
    float heightAtEnd;       // Y at the high end
    int rampAxis;            // 0 = ramps along X, 2 = ramps along Z
    bool rampPositive;       // true = height increases with positive axis
    float slopeAngle;        // angle in radians

    float heightAt(float x, float z) const;
    bool containsXZ(float x, float z) const;
    Vec3 surfaceNormal() const;
};

class World; // forward declaration

// Contact from geometry intersection
struct Contact {
    Vec3 normal;        // surface normal pointing away from geometry
    float penetration;  // how far the body is inside the surface
};

// Function type for terrain height queries
using TerrainHeightFn = float(*)(float, float, const void*);

class Player {
public:
    Player();

    void update(const InputState& input, float dt,
                const std::vector<AABB>& colliders,
                const std::vector<SlopeCollider>& slopes);

    // Set the terrain height query function (for heightmap-based terrain)
    void setTerrainQuery(const World* world);

    Vec3 position() const { return m_pos; }
    void setPosition(const Vec3& pos) { m_pos = pos; }
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
    void setYaw(float y) { m_yaw = y; }
    void setPitch(float p) { m_pitch = p; }

    Vec3 eyePosition() const;
    Vec3 forward() const;
    Vec3 right() const;

    const SceneObject& sceneObject() const { return m_sceneObj; }

    // Inventory (weapons/items)
    Inventory& inventory() { return m_inventory; }
    const Inventory& inventory() const { return m_inventory; }

    float radius() const { return m_radius; }
    float height() const { return m_height; }

    // Tuning
    float moveSpeed = 5.0f;
    float mouseSensitivity = 0.003f;

    // Min Y-component of contact normal to count as "walkable ground"
    // cos(45 deg) ~= 0.707; cos(37 deg) ~= 0.8
    static constexpr float minGroundNormalY = 0.7f;

private:
    Vec3 m_pos;
    Vec3 m_velocity;    // full 3D velocity
    float m_yaw;
    float m_pitch;

    float m_radius = 0.3f;
    float m_height = 1.8f;
    float m_eyeOffset = 1.6f;
    float m_standingEyeOffset = 1.6f;
    float m_crouchEyeOffset = 0.9f;
    bool m_crouching = false;

    bool m_onGround = false;

    Inventory m_inventory;

    Mesh m_mesh;
    SceneObject m_sceneObj;

    // Pointer to world for terrain queries (null = flat Y=0 ground)
    const World* m_world = nullptr;

    void updateTransform();

    // Get ground height at player's XZ position
    float getGroundHeight() const;
    Vec3 getGroundNormal() const;

    // Gather all contacts between the player body and world geometry
    void gatherContacts(const std::vector<AABB>& colliders,
                        const std::vector<SlopeCollider>& slopes,
                        std::vector<Contact>& contacts) const;

    // Check collision between player cylinder and an AABB, return contact if penetrating
    bool checkAABBContact(const AABB& box, Contact& contact) const;

    // Check collision with slope surface
    bool checkSlopeContact(const SlopeCollider& slope, Contact& contact) const;

    // Check collision with ground (terrain heightmap or Y=0)
    bool checkGroundContact(Contact& contact) const;
};
