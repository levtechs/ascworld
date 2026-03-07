#pragma once
#include <vector>
#include "core/vec_math.h"
#include "rendering/mesh.h"
#include "rendering/renderer.h"
#include "world/prefabs/city_materials.h"

// Place a neon sign board attached to a building wall.
// wallNormal: outward normal of the wall face (e.g. Vec3{1,0,0} for +X-facing wall).
// basePos:    world position of the sign's center bottom edge.
// Emits a dim PointLight in the accent colour.
void placeSignBoard(
    const Vec3& basePos, const Vec3& wallNormal,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights
);
