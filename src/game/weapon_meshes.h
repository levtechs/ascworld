#pragma once
#include "game/item.h"
#include "rendering/mesh.h"
#include "core/vec_math.h"
#include <vector>

// Generates and caches weapon meshes, provides SceneObjects for different view modes.
// Each weapon is composed of multiple mesh primitives (cubes, cylinders, spheres).
//
// Usage:
//   WeaponMeshes wm;
//   // First-person: weapon held in front of camera
//   auto objs = wm.getHeldObjects(ItemType::Saber, eyePos, yaw, pitch, attackProgress);
//   // Third-person: weapon on another player's body
//   auto objs = wm.getThirdPersonObjects(ItemType::Laser, playerPos, yaw);
//   // Dropped on ground
//   auto objs = wm.getDroppedObjects(ItemType::Flashbang, worldPos);

class WeaponMeshes {
public:
    WeaponMeshes();

    // Get scene objects for first-person held weapon view.
    // attackProgress: 0 = idle, 0..1 = attack animation phase
    std::vector<SceneObject> getHeldObjects(ItemType type, const Vec3& eyePos,
                                             float yaw, float pitch,
                                             float attackProgress = 0.0f) const;

    // Get scene objects for third-person weapon (attached to remote player).
    // attackProgress: 0 or negative = idle, 0..1 = attack animation
    std::vector<SceneObject> getThirdPersonObjects(ItemType type, const Vec3& playerPos,
                                                    float yaw, float pitch, float attackProgress = 0.0f) const;

    // Get scene objects for a dropped weapon on the ground.
    std::vector<SceneObject> getDroppedObjects(ItemType type, const Vec3& worldPos,
                                                float time = 0.0f) const;

    // Get scene objects for laser beam visual (thin bright line from start to end)
    std::vector<SceneObject> getLaserBeamObjects(const Vec3& start, const Vec3& end) const;

private:
    // Saber meshes: handle + blade
    Mesh m_saberHandle;
    Mesh m_saberBlade;
    Material m_saberHandleMat;
    Material m_saberBladeMat;

    // Laser meshes: body + barrel + grip
    Mesh m_laserBody;
    Mesh m_laserBarrel;
    Mesh m_laserGrip;
    Material m_laserBodyMat;
    Material m_laserBarrelMat;
    Material m_laserGripMat;

    // Flashbang meshes: sphere body + band
    Mesh m_flashbangBody;
    Mesh m_flashbangBand;
    Material m_flashbangBodyMat;
    Material m_flashbangBandMat;

    // Helper: compute the base transform for a held weapon using camera vectors
    // so the weapon is always fixed in the corner of the screen regardless of turning
    Mat4 heldBaseTransform(const Vec3& eyePos, float yaw, float pitch) const;

    // Laser beam mesh (thin stretched cube for beam visual)
    Mesh m_laserBeamMesh;
    Material m_laserBeamMat;
};
