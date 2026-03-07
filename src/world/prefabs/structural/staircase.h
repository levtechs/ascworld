#pragma once
#include <vector>
#include "rendering/mesh.h"
#include "game/player.h"
#include "world/prefabs/city_materials.h"

void placeStaircase(float x, float groundY, float z, float width, float totalHeight, float depth,
    int axis, bool ascending, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects,
    std::vector<AABB>& colliders,
    std::vector<SlopeCollider>& slopeColliders);
