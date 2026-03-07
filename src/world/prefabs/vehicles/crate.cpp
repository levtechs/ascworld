#include "world/prefabs/vehicles/crate.h"

static const float kCrateSize = 0.7f;

void placeCrate(
    float x, float groundY, float z, float yawRad,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<AABB>& colliders)
{
    // Slightly warm/brown tint on the metal palette entry for a scavenged crate
    Material crateMat = pal.metal;
    crateMat.color.r *= 1.15f;  // nudge toward rust-brown
    crateMat.color.g *= 0.90f;

    float half = kCrateSize * 0.5f;
    float cy   = groundY + half;

    Mat4 transform = Mat4::translate({ x, cy, z })
                   * Mat4::rotateY(yawRad)
                   * Mat4::scale({ kCrateSize, kCrateSize, kCrateSize });

    objects.push_back({ meshCache.cube(), transform, crateMat });

    // AABB in world space — use full extent (yaw ignored; box is cubic)
    colliders.push_back(AABB(
        { x - half, groundY,        z - half },
        { x + half, groundY + kCrateSize, z + half }
    ));
}
