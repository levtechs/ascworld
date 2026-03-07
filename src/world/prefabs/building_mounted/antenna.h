#pragma once
#include <vector>
#include "rendering/mesh.h"
#include "rendering/renderer.h"
#include "world/prefabs/city_materials.h"

// Place a rooftop antenna with its base at (x, rooftopY, z).
// No collider — antennas sit on rooftops the player cannot easily reach.
void placeAntenna(
    float x, float rooftopY, float z,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects
);
