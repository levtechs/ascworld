#pragma once
#include "core/chunk.h"
#include "core/noise.h"
#include "rendering/mesh.h"
#include "rendering/renderer.h" // for PointLight
#include "game/player.h"   // for AABB
#include <vector>
#include <deque>

class Terrain; // forward declaration

// Structure generation: ruins, pillars, lights
class Structures {
public:
    static constexpr float CHUNK_SIZE = 16.0f;

    Structures();

    void init(const PerlinNoise& noise, uint32_t seed,
              Mesh* cubeMesh, Mesh* ruinPillarMesh);

    // Generate ruins for a chunk
    void generateRuins(const Chunk& chunk,
                       const Terrain& terrain,
                       std::deque<Mesh>& meshes,
                       std::vector<SceneObject>& objects,
                       std::vector<AABB>& colliders);

    // Generate atmospheric lights for a chunk
    void generateLights(const Chunk& chunk,
                        const Terrain& terrain,
                        std::vector<PointLight>& lights);

private:
    const PerlinNoise* m_noise = nullptr;
    uint32_t m_seed = 0;

    // Shared mesh prototypes (owned by World)
    Mesh* m_cubeMesh = nullptr;
    Mesh* m_ruinPillarMesh = nullptr;

    void addBox(const Vec3& pos, const Vec3& size, const Material& mat,
                bool noCollision,
                std::vector<SceneObject>& objects,
                std::vector<AABB>& colliders);

    void addRuinWall(const Vec3& pos, const Vec3& size, const Material& mat,
                     std::vector<SceneObject>& objects,
                     std::vector<AABB>& colliders);

    void addRuinPillar(const Vec3& pos, float height, float radius,
                       std::vector<SceneObject>& objects,
                       std::vector<AABB>& colliders);
};
