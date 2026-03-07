#pragma once
#include "core/chunk.h"
#include "core/noise.h"
#include "rendering/mesh.h"
#include "rendering/renderer.h" // for PointLight
#include "game/player.h"   // for AABB, SlopeCollider
#include "world/terrain.h"
#include "world/vegetation.h"
#include "world/structures.h"
#include <vector>
#include <deque>
#include <unordered_map>
#include <cstdint>

class World {
public:
    // World dimensions
    static constexpr float CHUNK_SIZE = 16.0f;
    static constexpr int WORLD_CHUNKS = 16; // 16x16 grid of chunks

    World();

    // Generate the world from a seed
    void generate(uint32_t seed);

    // Clear all generated data
    void clear();

    // Access generated data for rendering and physics
    const std::vector<SceneObject>& objects() const { return m_objects; }
    const std::vector<PointLight>& lights() const { return m_lights; }
    const std::vector<AABB>& colliders() const { return m_colliders; }
    const std::vector<SlopeCollider>& slopes() const { return m_slopes; }

    // Terrain queries (delegates to Terrain subsystem)
    float terrainHeightAt(float wx, float wz) const;
    float effectiveHeightAt(float wx, float wz) const;
    Vec3 terrainNormalAt(float wx, float wz) const;
    bool isUnderwater(float wx, float wz) const;

    // Constants exposed for other systems
    float waterLevel() const { return Terrain::WATER_LEVEL; }
    float worldSize() const { return CHUNK_SIZE * WORLD_CHUNKS; }

private:
    // Noise generator
    PerlinNoise m_noise;

    // Sub-generators
    Terrain m_terrain;
    Vegetation m_vegetation;
    Structures m_structures;

    // All generated data (stable storage for mesh pointers)
    std::deque<Mesh> m_meshes;
    std::vector<SceneObject> m_objects;
    std::vector<PointLight> m_lights;
    std::vector<AABB> m_colliders;
    std::vector<SlopeCollider> m_slopes;

    // Chunk map
    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> m_chunks;

    // Shared mesh prototypes
    Mesh m_cubeMesh;
    Mesh m_treeTrunkMesh;
    Mesh m_treeCanopyMesh;
    Mesh m_ruinPillarMesh;

    // Generate a single chunk
    void generateChunk(int cx, int cz);
};
