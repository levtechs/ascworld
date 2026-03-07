#pragma once
#include <vector>
#include "rendering/mesh.h"
#include "rendering/renderer.h"
#include "game/player.h"
#include "world/prefabs/city_materials.h"

void placeDumpster(
    float x, float groundY, float z, float yawRad,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights,
    std::vector<AABB>& colliders
);
