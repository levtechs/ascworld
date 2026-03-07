#pragma once
#include "core/chunk.h"
#include "core/noise.h"
#include "rendering/mesh.h"
#include "rendering/framebuffer.h" // for Color3
#include <vector>
#include <deque>

// Forward declarations for types we push into
struct SceneObject;
struct AABB;
struct PointLight;

// Terrain generation and height query system
class Terrain {
public:
    // World constants
    static constexpr float CHUNK_SIZE = 16.0f;
    static constexpr float WATER_LEVEL = 1.8f;

    Terrain();

    void init(const PerlinNoise& noise, uint32_t seed);

    // Generate terrain mesh and heightmap for a chunk
    void generateTerrain(Chunk& chunk,
                         std::deque<Mesh>& meshes,
                         std::vector<SceneObject>& objects);

    // Generate water planes for low-lying areas in a chunk
    void generateWater(const Chunk& chunk,
                       std::deque<Mesh>& meshes,
                       std::vector<SceneObject>& objects);

    // Raw terrain height from noise (no chunk lookup, always available)
    float rawTerrainHeight(float wx, float wz) const;

    // Compute slope magnitude at a world point
    float slopeAt(float wx, float wz) const;

    // Get terrain material color based on height, slope, and position
    Color3 terrainColor(float height, float slope, float wx, float wz) const;

private:
    const PerlinNoise* m_noise = nullptr;
    uint32_t m_seed = 0;
};
