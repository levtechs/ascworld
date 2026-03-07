#pragma once
#include <cstdint>
#include <vector>
#include "rendering/mesh.h"
#include "game/player.h"
#include "world/prefabs/city_materials.h"

void placeRubblePile(float x, float groundY, float z, uint32_t seed, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects, std::vector<AABB>& colliders);
