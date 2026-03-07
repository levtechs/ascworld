#include "world/world.h"
#include <cmath>
#include <algorithm>

World::World() {}

void World::clear() {
    m_meshes.clear();
    m_objects.clear();
    m_lights.clear();
    m_colliders.clear();
    m_slopes.clear();
    m_chunks.clear();
}

void World::generate(uint32_t seed) {
    clear();

    // Initialize noise
    m_noise = PerlinNoise(seed);

    // Create shared meshes
    m_cubeMesh = createCube();
    m_treeTrunkMesh = createCylinder(0.15f, 1.0f, 6);
    m_treeCanopyMesh = createSphere(6, 8, 1.0f);
    m_ruinPillarMesh = createCylinder(0.25f, 1.0f, 8);

    // Initialize sub-generators
    m_terrain.init(m_noise, seed);
    m_vegetation.init(m_noise, seed, &m_treeTrunkMesh, &m_treeCanopyMesh);
    m_structures.init(m_noise, seed, &m_cubeMesh, &m_ruinPillarMesh);

    // Reserve space (rough estimate)
    m_objects.reserve(WORLD_CHUNKS * WORLD_CHUNKS * 20);
    m_colliders.reserve(WORLD_CHUNKS * WORLD_CHUNKS * 10);
    m_lights.reserve(WORLD_CHUNKS * WORLD_CHUNKS);

    // Generate all chunks
    for (int cz = 0; cz < WORLD_CHUNKS; cz++) {
        for (int cx = 0; cx < WORLD_CHUNKS; cx++) {
            generateChunk(cx, cz);
        }
    }
}

void World::generateChunk(int cx, int cz) {
    Chunk& chunk = m_chunks[ChunkCoord{cx, cz}];
    chunk.coord = {cx, cz};

    // Track where this chunk's data starts in global arrays
    chunk.objectStart = m_objects.size();
    chunk.colliderStart = m_colliders.size();
    chunk.lightStart = m_lights.size();

    // Generate terrain (fills heightmap + terrain mesh)
    m_terrain.generateTerrain(chunk, m_meshes, m_objects);

    // Generate water planes for low areas
    m_terrain.generateWater(chunk, m_meshes, m_objects);

    // Generate vegetation (trees, rocks)
    m_vegetation.generateTrees(chunk, m_terrain, m_meshes, m_objects, m_colliders);
    m_vegetation.generateRocks(chunk, m_terrain, m_meshes, m_objects, m_colliders);

    // Generate structures (ruins, pillars)
    m_structures.generateRuins(chunk, m_terrain, m_meshes, m_objects, m_colliders);
    m_structures.generateLights(chunk, m_terrain, m_lights);

    // Record how many items this chunk produced
    chunk.objectCount = m_objects.size() - chunk.objectStart;
    chunk.colliderCount = m_colliders.size() - chunk.colliderStart;
    chunk.lightCount = m_lights.size() - chunk.lightStart;
}

float World::terrainHeightAt(float wx, float wz) const {
    // Try chunk lookup first for heightmap interpolation
    int cx = (int)std::floor(wx / CHUNK_SIZE);
    int cz = (int)std::floor(wz / CHUNK_SIZE);

    auto it = m_chunks.find(ChunkCoord{cx, cz});
    if (it != m_chunks.end()) {
        float localX = wx - cx * CHUNK_SIZE;
        float localZ = wz - cz * CHUNK_SIZE;
        return it->second.heightAt(localX, localZ, CHUNK_SIZE);
    }

    // Fallback: compute from noise directly
    return m_terrain.rawTerrainHeight(wx, wz);
}

float World::effectiveHeightAt(float wx, float wz) const {
    float h = terrainHeightAt(wx, wz);
    // If terrain is below water, standing height is water level
    if (h < Terrain::WATER_LEVEL) {
        return Terrain::WATER_LEVEL;
    }
    return h;
}

Vec3 World::terrainNormalAt(float wx, float wz) const {
    float eps = 0.3f;
    float hC = terrainHeightAt(wx, wz);
    float hE = terrainHeightAt(wx + eps, wz);
    float hW = terrainHeightAt(wx - eps, wz);
    float hN = terrainHeightAt(wx, wz - eps);
    float hS = terrainHeightAt(wx, wz + eps);

    Vec3 tangentX(2.0f * eps, hE - hW, 0.0f);
    Vec3 tangentZ(0.0f, hS - hN, 2.0f * eps);
    return tangentZ.cross(tangentX).normalized();
}

bool World::isUnderwater(float wx, float wz) const {
    return terrainHeightAt(wx, wz) < Terrain::WATER_LEVEL;
}
