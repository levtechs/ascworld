#pragma once
#include "core/chunk.h"
#include "core/noise.h"
#include "rendering/mesh.h"
#include "game/player.h" // for AABB
#include <vector>
#include <deque>

struct SceneObject;

class Terrain; // forward declaration for height/slope queries

// Vegetation generation: trees and rocks
class Vegetation {
public:
    static constexpr float CHUNK_SIZE = 16.0f;

    Vegetation();

    void init(const PerlinNoise& noise, uint32_t seed,
              Mesh* treeTrunkMesh, Mesh* treeCanopyMesh);

    // Generate trees for a chunk
    void generateTrees(const Chunk& chunk,
                       const Terrain& terrain,
                       std::deque<Mesh>& meshes,
                       std::vector<SceneObject>& objects,
                       std::vector<AABB>& colliders);

    // Generate rocks/boulders for a chunk
    void generateRocks(const Chunk& chunk,
                       const Terrain& terrain,
                       std::deque<Mesh>& meshes,
                       std::vector<SceneObject>& objects,
                       std::vector<AABB>& colliders);

private:
    const PerlinNoise* m_noise = nullptr;
    uint32_t m_seed = 0;

    // Shared mesh prototypes (owned by World, we just hold pointers)
    Mesh* m_treeTrunkMesh = nullptr;
    Mesh* m_treeCanopyMesh = nullptr;

    void addTree(const Vec3& pos, float trunkHeight, float canopyRadius,
                 std::vector<SceneObject>& objects,
                 std::vector<AABB>& colliders);

    void addRock(const Vec3& pos, float size,
                 std::deque<Mesh>& meshes,
                 std::vector<SceneObject>& objects,
                 std::vector<AABB>& colliders);
};
