#pragma once
#include "core/vec_math.h"
#include "core/chunk.h"
#include "rendering/mesh.h"
#include "rendering/renderer.h"
#include "rendering/framebuffer.h"
#include "game/player.h"
#include "world/prefabs/city_materials.h"
#include "world/buildings.h"
#include <deque>
#include <vector>
#include <unordered_map>
#include <cstdint>

class CityLayout {
public:
    static constexpr float CHUNK_SIZE = 16.0f;
    static constexpr int WORLD_CHUNKS = 16;
    static constexpr float FLOOR_HEIGHT = 3.0f;

    void generate(uint32_t seed,
                  MeshCache& meshCache,
                  std::vector<SceneObject>& objects,
                  std::vector<PointLight>& lights,
                  std::vector<AABB>& colliders,
                  std::vector<SlopeCollider>& slopeColliders);

    // Get district type at world position
    DistrictType districtAt(float wx, float wz) const;

    // Get the DistrictPalette for a world position
    const DistrictPalette& paletteAt(float wx, float wz) const;

    // Ground height is always 0 in the city
    float groundHeight() const { return 0.0f; }

private:
    uint32_t m_seed = 0;

    // District map (per chunk)
    DistrictType m_districtMap[16][16];

    // Cached palettes
    DistrictPalette m_palettes[6]; // indexed by DistrictType

    void assignDistricts(uint32_t seed);
    void generateStreets(uint32_t seed, MeshCache& meshCache, std::vector<SceneObject>& objects, std::vector<PointLight>& lights, std::vector<AABB>& colliders);
    void generateBlock(int blockX, int blockZ, float x0, float z0, float x1, float z1, uint32_t seed, MeshCache& meshCache, std::vector<SceneObject>& objects, std::vector<PointLight>& lights, std::vector<AABB>& colliders, std::vector<SlopeCollider>& slopeColliders);
    void generateGroundPlane(MeshCache& meshCache, std::vector<SceneObject>& objects, std::vector<AABB>& colliders);
};
