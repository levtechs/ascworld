#pragma once
#include <vector>
#include "core/vec_math.h"
#include "rendering/mesh.h"
#include "rendering/renderer.h"
#include "world/prefabs/city_materials.h"

// Place a run of industrial pipe.
// origin: world-space start of the pipe.
// axis:   0 = along X, 2 = along Z.
// length: total pipe length in world units.
// Brackets are placed every 1.5 units along the pipe length.
// No collider — pipes are wall-mounted and thin.
void placePipeRun(
    const Vec3& origin, int axis, float length,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects
);
