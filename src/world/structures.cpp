#include "world/structures.h"
#include "world/terrain.h"
#include <cmath>
#include <algorithm>

Structures::Structures() {}

void Structures::init(const PerlinNoise& noise, uint32_t seed,
                      Mesh* cubeMesh, Mesh* ruinPillarMesh) {
    m_noise = &noise;
    m_seed = seed;
    m_cubeMesh = cubeMesh;
    m_ruinPillarMesh = ruinPillarMesh;
}

void Structures::generateRuins(const Chunk& chunk,
                                const Terrain& terrain,
                                std::deque<Mesh>& meshes,
                                std::vector<SceneObject>& objects,
                                std::vector<AABB>& colliders) {
    int cx = chunk.coord.x;
    int cz = chunk.coord.z;

    float centerX = cx * CHUNK_SIZE + CHUNK_SIZE * 0.5f;
    float centerZ = cz * CHUNK_SIZE + CHUNK_SIZE * 0.5f;

    // Don't place ruins on steep/high terrain or underwater
    float avgHeight = terrain.rawTerrainHeight(centerX, centerZ);
    if (avgHeight < Terrain::WATER_LEVEL + 1.0f) return;
    if (avgHeight > 10.0f) return;
    float avgSlope = terrain.slopeAt(centerX, centerZ);
    if (avgSlope > 1.5f) return;

    float ruinNoise = m_noise->fbm(centerX * 0.025f + 200.0f, centerZ * 0.025f + 200.0f, 2);

    // Only place ruins in certain areas
    if (ruinNoise < 0.2f) return;

    float intensity = (ruinNoise - 0.2f) / 0.8f;
    int numStructures = (int)(intensity * 3.0f + 0.5f);

    Material ruinStoneMat(Color3(0.5f, 0.47f, 0.42f), 8.0f, 0.1f);
    Material ruinDarkMat(Color3(0.38f, 0.35f, 0.32f), 8.0f, 0.08f);
    Material ruinMossyMat(Color3(0.35f, 0.42f, 0.3f), 4.0f, 0.05f);

    for (int i = 0; i < numStructures; i++) {
        float lx = chunkRandomFloat(m_seed, cx, cz, 100 + i * 10) * (CHUNK_SIZE - 4.0f) + 2.0f;
        float lz = chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 1) * (CHUNK_SIZE - 4.0f) + 2.0f;
        float wx = cx * CHUNK_SIZE + lx;
        float wz = cz * CHUNK_SIZE + lz;
        float terrainY = terrain.rawTerrainHeight(wx, wz);

        if (terrainY < Terrain::WATER_LEVEL + 0.5f) continue;

        uint32_t structType = chunkRandom(m_seed, cx, cz, 100 + i * 10 + 2) % 3;

        if (structType == 0) {
            // Single broken wall
            float wallHeight = 1.0f + chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 3) * 2.5f;
            float wallWidth = 2.0f + chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 4) * 3.0f;
            float wallThick = 0.3f + chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 5) * 0.2f;

            bool rotated = (chunkRandom(m_seed, cx, cz, 100 + i * 10 + 6) % 2) == 0;
            Vec3 size = rotated ? Vec3(wallThick, wallHeight, wallWidth)
                                : Vec3(wallWidth, wallHeight, wallThick);

            addRuinWall(Vec3(wx, terrainY + wallHeight * 0.5f, wz), size, ruinStoneMat,
                        objects, colliders);

        } else if (structType == 1) {
            // Pillar cluster
            int numPillars = 2 + (int)(chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 3) * 2.5f);
            for (int p = 0; p < numPillars; p++) {
                float px = wx + (chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 20 + p * 2) - 0.5f) * 3.0f;
                float pz = wz + (chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 21 + p * 2) - 0.5f) * 3.0f;
                float py = terrain.rawTerrainHeight(px, pz);
                float pillarH = 1.5f + chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 30 + p) * 3.0f;
                float pillarR = 0.2f + chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 40 + p) * 0.15f;
                addRuinPillar(Vec3(px, py, pz), pillarH, pillarR, objects, colliders);
            }

        } else {
            // L-shaped corner ruin
            float wallHeight = 1.5f + chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 3) * 2.0f;
            float wallLen1 = 2.0f + chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 4) * 2.0f;
            float wallLen2 = 2.0f + chunkRandomFloat(m_seed, cx, cz, 100 + i * 10 + 5) * 2.0f;
            float wallThick = 0.3f;

            addRuinWall(Vec3(wx + wallLen1 * 0.5f, terrainY + wallHeight * 0.5f, wz),
                        Vec3(wallLen1, wallHeight, wallThick), ruinDarkMat,
                        objects, colliders);
            addRuinWall(Vec3(wx, terrainY + wallHeight * 0.5f, wz + wallLen2 * 0.5f),
                        Vec3(wallThick, wallHeight * 0.8f, wallLen2), ruinMossyMat,
                        objects, colliders);
        }
    }
}

void Structures::generateLights(const Chunk& chunk,
                                 const Terrain& terrain,
                                 std::vector<PointLight>& lights) {
    int cx = chunk.coord.x;
    int cz = chunk.coord.z;

    float centerX = cx * CHUNK_SIZE + CHUNK_SIZE * 0.5f;
    float centerZ = cz * CHUNK_SIZE + CHUNK_SIZE * 0.5f;
    float ruinNoise = m_noise->fbm(centerX * 0.025f + 200.0f, centerZ * 0.025f + 200.0f, 2);

    if (ruinNoise < 0.3f) return;

    float lx = chunkRandomFloat(m_seed, cx, cz, 500) * CHUNK_SIZE;
    float lz = chunkRandomFloat(m_seed, cx, cz, 501) * CHUNK_SIZE;
    float wx = cx * CHUNK_SIZE + lx;
    float wz = cz * CHUNK_SIZE + lz;
    float terrainY = terrain.rawTerrainHeight(wx, wz);

    if (terrainY < Terrain::WATER_LEVEL + 0.5f) return;

    float r = 0.7f + chunkRandomFloat(m_seed, cx, cz, 502) * 0.3f;
    float g = 0.5f + chunkRandomFloat(m_seed, cx, cz, 503) * 0.4f;
    float b = 0.2f + chunkRandomFloat(m_seed, cx, cz, 504) * 0.2f;
    lights.push_back(PointLight(
        Vec3(wx, terrainY + 2.5f, wz),
        Color3(r, g, b), 1.2f, 10.0f
    ));
}

void Structures::addBox(const Vec3& pos, const Vec3& size, const Material& mat,
                         bool noCollision,
                         std::vector<SceneObject>& objects,
                         std::vector<AABB>& colliders) {
    Mat4 transform = Mat4::translate(pos) * Mat4::scale(size);
    objects.push_back({m_cubeMesh, transform, mat});

    if (!noCollision) {
        Vec3 halfSize(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f);
        colliders.push_back(AABB(
            Vec3(pos.x - halfSize.x, pos.y - halfSize.y, pos.z - halfSize.z),
            Vec3(pos.x + halfSize.x, pos.y + halfSize.y, pos.z + halfSize.z)
        ));
    }
}

void Structures::addRuinWall(const Vec3& pos, const Vec3& size, const Material& mat,
                              std::vector<SceneObject>& objects,
                              std::vector<AABB>& colliders) {
    addBox(pos, size, mat, false, objects, colliders);
}

void Structures::addRuinPillar(const Vec3& pos, float height, float radius,
                                std::vector<SceneObject>& objects,
                                std::vector<AABB>& colliders) {
    Material pillarMat(Color3(0.5f, 0.48f, 0.44f), 8.0f, 0.1f);

    Mat4 transform = Mat4::translate(pos)
                   * Mat4::scale(Vec3(radius / 0.25f, height, radius / 0.25f));
    objects.push_back({m_ruinPillarMesh, transform, pillarMat});

    colliders.push_back(AABB(
        Vec3(pos.x - radius, pos.y, pos.z - radius),
        Vec3(pos.x + radius, pos.y + height, pos.z + radius)
    ));
}
