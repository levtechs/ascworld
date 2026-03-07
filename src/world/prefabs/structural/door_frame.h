#pragma once
#include <vector>
#include "rendering/mesh.h"
#include "game/player.h"
#include "world/prefabs/city_materials.h"

void placeDoorFrame(float x, float groundY, float z, float yawRad,
    float wallWidth, float wallHeight, float wallDepth, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects, std::vector<AABB>& colliders);
