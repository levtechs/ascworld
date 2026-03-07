#pragma once
#include <vector>
#include "rendering/mesh.h"
#include "game/player.h"
#include "world/prefabs/city_materials.h"

void placePillar(float x, float groundY, float z, float height, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects, std::vector<AABB>& colliders);
