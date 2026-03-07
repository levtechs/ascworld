#include "world/vegetation.h"
#include "world/terrain.h"
#include <cmath>
#include <algorithm>

Vegetation::Vegetation() {}

void Vegetation::init(const PerlinNoise& noise, uint32_t seed,
                      Mesh* treeTrunkMesh, Mesh* treeCanopyMesh) {
    m_noise = &noise;
    m_seed = seed;
    m_treeTrunkMesh = treeTrunkMesh;
    m_treeCanopyMesh = treeCanopyMesh;
}

void Vegetation::generateTrees(const Chunk& chunk,
                                const Terrain& terrain,
                                std::deque<Mesh>& meshes,
                                std::vector<SceneObject>& objects,
                                std::vector<AABB>& colliders) {
    int cx = chunk.coord.x;
    int cz = chunk.coord.z;

    // Use noise to determine tree density in this chunk
    float centerX = cx * CHUNK_SIZE + CHUNK_SIZE * 0.5f;
    float centerZ = cz * CHUNK_SIZE + CHUNK_SIZE * 0.5f;
    float density = m_noise->fbm(centerX * 0.03f + 100.0f, centerZ * 0.03f + 100.0f, 2);
    density = (density + 1.0f) * 0.5f; // [0, 1]

    // Check terrain height - no trees on mountains or underwater
    float avgHeight = terrain.rawTerrainHeight(centerX, centerZ);
    if (avgHeight < Terrain::WATER_LEVEL + 0.5f) return; // too low / underwater
    if (avgHeight > 12.0f) return; // too high for trees (above treeline)

    // Reduce density at higher elevations
    float elevationFactor = 1.0f;
    if (avgHeight > 8.0f) {
        elevationFactor = 1.0f - (avgHeight - 8.0f) / 4.0f;
        elevationFactor = std::max(0.0f, elevationFactor);
    }

    // 0-6 trees per chunk based on density and elevation
    int maxTrees = (int)(density * elevationFactor * 6.0f + 0.5f);

    for (int i = 0; i < maxTrees; i++) {
        float lx = chunkRandomFloat(m_seed, cx, cz, i * 4) * (CHUNK_SIZE - 2.0f) + 1.0f;
        float lz = chunkRandomFloat(m_seed, cx, cz, i * 4 + 1) * (CHUNK_SIZE - 2.0f) + 1.0f;
        float wx = cx * CHUNK_SIZE + lx;
        float wz = cz * CHUNK_SIZE + lz;

        float terrainY = terrain.rawTerrainHeight(wx, wz);

        // Don't place on steep terrain or underwater
        if (terrainY < Terrain::WATER_LEVEL + 0.3f) continue;

        float slope = terrain.slopeAt(wx, wz);
        if (slope > 1.5f) continue;

        // Random tree parameters
        float trunkHeight = 1.5f + chunkRandomFloat(m_seed, cx, cz, i * 4 + 2) * 2.5f;
        float canopyRadius = 0.8f + chunkRandomFloat(m_seed, cx, cz, i * 4 + 3) * 1.2f;

        // Smaller trees at higher elevation
        if (terrainY > 8.0f) {
            float shrinkFactor = 1.0f - (terrainY - 8.0f) / 6.0f;
            shrinkFactor = std::max(0.3f, shrinkFactor);
            trunkHeight *= shrinkFactor;
            canopyRadius *= shrinkFactor;
        }

        addTree(Vec3(wx, terrainY, wz), trunkHeight, canopyRadius, objects, colliders);
    }
}

void Vegetation::generateRocks(const Chunk& chunk,
                                const Terrain& terrain,
                                std::deque<Mesh>& meshes,
                                std::vector<SceneObject>& objects,
                                std::vector<AABB>& colliders) {
    int cx = chunk.coord.x;
    int cz = chunk.coord.z;

    // Rocks appear more on steep terrain and at higher elevations
    float centerX = cx * CHUNK_SIZE + CHUNK_SIZE * 0.5f;
    float centerZ = cz * CHUNK_SIZE + CHUNK_SIZE * 0.5f;
    float avgHeight = terrain.rawTerrainHeight(centerX, centerZ);
    float avgSlope = terrain.slopeAt(centerX, centerZ);

    // Rock density increases with height and slope
    float rockDensity = 0.0f;
    if (avgHeight > 6.0f) rockDensity += (avgHeight - 6.0f) / 12.0f;
    if (avgSlope > 0.8f) rockDensity += (avgSlope - 0.8f) * 0.3f;

    // Add some noise-based variation
    float rockNoise = m_noise->fbm(centerX * 0.04f + 400.0f, centerZ * 0.04f + 400.0f, 2);
    rockDensity += (rockNoise + 1.0f) * 0.15f;

    int numRocks = (int)(rockDensity * 4.0f);
    numRocks = std::min(numRocks, 5);

    for (int i = 0; i < numRocks; i++) {
        float lx = chunkRandomFloat(m_seed, cx, cz, 200 + i * 3) * (CHUNK_SIZE - 2.0f) + 1.0f;
        float lz = chunkRandomFloat(m_seed, cx, cz, 200 + i * 3 + 1) * (CHUNK_SIZE - 2.0f) + 1.0f;
        float wx = cx * CHUNK_SIZE + lx;
        float wz = cz * CHUNK_SIZE + lz;

        float terrainY = terrain.rawTerrainHeight(wx, wz);
        if (terrainY < Terrain::WATER_LEVEL + 0.2f) continue;

        float rockSize = 0.3f + chunkRandomFloat(m_seed, cx, cz, 200 + i * 3 + 2) * 1.2f;
        // Bigger rocks at higher elevation
        if (terrainY > 10.0f) {
            rockSize *= 1.0f + (terrainY - 10.0f) * 0.1f;
        }

        addRock(Vec3(wx, terrainY, wz), rockSize, meshes, objects, colliders);
    }
}

void Vegetation::addTree(const Vec3& pos, float trunkHeight, float canopyRadius,
                          std::vector<SceneObject>& objects,
                          std::vector<AABB>& colliders) {
    // Trunk: brown cylinder
    Material trunkMat(Color3(0.4f, 0.25f, 0.1f), 4.0f, 0.05f);
    float trunkRadius = 0.1f + canopyRadius * 0.08f;

    Mat4 trunkTransform = Mat4::translate(pos)
                        * Mat4::scale(Vec3(trunkRadius / 0.15f, trunkHeight, trunkRadius / 0.15f));
    objects.push_back({m_treeTrunkMesh, trunkTransform, trunkMat});

    // Trunk collider
    colliders.push_back(AABB(
        Vec3(pos.x - trunkRadius, pos.y, pos.z - trunkRadius),
        Vec3(pos.x + trunkRadius, pos.y + trunkHeight, pos.z + trunkRadius)
    ));

    // Canopy: green sphere
    Material canopyMat(Color3(0.15f + canopyRadius * 0.05f, 0.4f + canopyRadius * 0.05f, 0.1f), 4.0f, 0.05f);
    float canopyY = pos.y + trunkHeight + canopyRadius * 0.6f;

    Mat4 canopyTransform = Mat4::translate(Vec3(pos.x, canopyY, pos.z))
                         * Mat4::scale(Vec3(canopyRadius, canopyRadius * 0.8f, canopyRadius));
    objects.push_back({m_treeCanopyMesh, canopyTransform, canopyMat});
}

void Vegetation::addRock(const Vec3& pos, float size,
                          std::deque<Mesh>& meshes,
                          std::vector<SceneObject>& objects,
                          std::vector<AABB>& colliders) {
    // Use a squashed sphere for rocks
    // Create a low-poly sphere for each rock (varied shape)
    Mesh rockMesh = createSphere(4, 6, 1.0f);
    meshes.push_back(std::move(rockMesh));
    Mesh* meshPtr = &meshes.back();

    // Randomly squash/stretch the rock
    float sx = size * (0.7f + 0.6f * ((float)((int)(pos.x * 13.7f) & 0xFF) / 255.0f));
    float sy = size * (0.4f + 0.4f * ((float)((int)(pos.z * 17.3f) & 0xFF) / 255.0f));
    float sz = size * (0.7f + 0.6f * ((float)((int)((pos.x + pos.z) * 7.1f) & 0xFF) / 255.0f));

    Mat4 transform = Mat4::translate(Vec3(pos.x, pos.y + sy * 0.3f, pos.z))
                   * Mat4::scale(Vec3(sx, sy, sz));

    // Rock color - grey with slight brown/green tint
    float tint = ((int)(pos.x * 23.1f + pos.z * 31.7f) & 0xFF) / 255.0f;
    Color3 rockColor(0.40f + tint * 0.1f, 0.38f + tint * 0.08f, 0.35f + tint * 0.05f);
    Material rockMat(rockColor, 8.0f, 0.15f);

    objects.push_back({meshPtr, transform, rockMat});

    // Simple AABB collider
    float maxR = std::max(sx, sz);
    colliders.push_back(AABB(
        Vec3(pos.x - maxR, pos.y, pos.z - maxR),
        Vec3(pos.x + maxR, pos.y + sy * 0.8f, pos.z + maxR)
    ));
}
