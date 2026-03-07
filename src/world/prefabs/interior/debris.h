#pragma once
#include <cstdint>
#include <vector>
#include "rendering/mesh.h"
#include "world/prefabs/city_materials.h"

void placeDebris(float x, float groundY, float z, uint32_t seed, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects);
