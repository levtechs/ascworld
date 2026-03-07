#include "world/terrain.h"
#include "rendering/mesh.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Terrain::Terrain() {}

void Terrain::init(const PerlinNoise& noise, uint32_t seed) {
    m_noise = &noise;
    m_seed = seed;
}

float Terrain::rawTerrainHeight(float wx, float wz) const {
    // === Multi-layer terrain ===

    // Layer 1: Continental shape - very low frequency, large-scale elevation zones
    float continental = m_noise->fbm(wx * 0.005f + 500.0f, wz * 0.005f + 500.0f, 3, 2.0f, 0.5f);
    // continental is in [-1, 1], map to [0, 1]
    float contFactor = (continental + 1.0f) * 0.5f;

    // Layer 2: Mountain ridges - medium frequency with high amplitude
    float ridge = m_noise->fbm(wx * 0.015f, wz * 0.015f, 5, 2.2f, 0.5f);
    // Create sharp ridges by taking absolute value and inverting
    ridge = 1.0f - std::abs(ridge);
    ridge = ridge * ridge; // sharpen the peaks
    float mountainHeight = ridge * 18.0f; // mountains up to ~18 units

    // Layer 3: Rolling hills - medium-low frequency
    float hills = m_noise->fbm(wx * 0.025f, wz * 0.025f, 4, 2.0f, 0.5f);
    float hillHeight = hills * 4.0f + 3.0f; // range roughly [-1, 7]

    // Layer 4: Fine detail - high frequency bumps
    float detail = m_noise->fbm(wx * 0.1f, wz * 0.1f, 3, 2.0f, 0.4f);
    float detailHeight = detail * 0.8f;

    // Combine layers based on continental factor
    // High continental = mountainous, low = flat plains/valleys
    float mountainBlend = std::max(0.0f, contFactor - 0.4f) / 0.6f; // 0 when cont < 0.4, ramps to 1
    mountainBlend = mountainBlend * mountainBlend; // make mountains rarer

    float h = hillHeight
            + mountainHeight * mountainBlend
            + detailHeight;

    // Create some valleys that go below water level for lakes
    float valleyNoise = m_noise->fbm(wx * 0.012f + 300.0f, wz * 0.012f + 300.0f, 3, 2.0f, 0.5f);
    if (valleyNoise < -0.2f) {
        float valleyDepth = (-0.2f - valleyNoise) / 0.8f; // 0 to 1
        h -= valleyDepth * 4.0f; // push terrain down to create lake basins
    }

    // Floor at 0
    return std::max(0.0f, h);
}

float Terrain::slopeAt(float wx, float wz) const {
    float eps = 0.5f;
    float hC = rawTerrainHeight(wx, wz);
    float hE = rawTerrainHeight(wx + eps, wz);
    float hW = rawTerrainHeight(wx - eps, wz);
    float hN = rawTerrainHeight(wx, wz - eps);
    float hS = rawTerrainHeight(wx, wz + eps);

    float dxSlope = (hE - hW) / (2.0f * eps);
    float dzSlope = (hS - hN) / (2.0f * eps);
    return std::sqrt(dxSlope * dxSlope + dzSlope * dzSlope);
}

Color3 Terrain::terrainColor(float height, float slope, float wx, float wz) const {
    // Base colors for different biomes
    Color3 deepWaterColor(0.08f, 0.15f, 0.35f);
    Color3 shallowWaterColor(0.12f, 0.25f, 0.40f);
    Color3 sandColor(0.65f, 0.58f, 0.40f);
    Color3 grassColor(0.20f, 0.40f, 0.13f);
    Color3 darkGrassColor(0.14f, 0.32f, 0.10f);
    Color3 rockColor(0.45f, 0.42f, 0.38f);
    Color3 darkRockColor(0.35f, 0.32f, 0.28f);
    Color3 snowColor(0.85f, 0.88f, 0.92f);

    Color3 result;

    if (height < WATER_LEVEL - 0.5f) {
        // Deep underwater
        result = deepWaterColor;
    } else if (height < WATER_LEVEL + 0.3f) {
        // Beach / shoreline
        float t = (height - (WATER_LEVEL - 0.5f)) / 0.8f;
        t = std::max(0.0f, std::min(1.0f, t));
        result = lerp(shallowWaterColor, sandColor, t);
    } else if (height < 5.0f) {
        // Lowlands - grass
        float t = (height - (WATER_LEVEL + 0.3f)) / (5.0f - WATER_LEVEL - 0.3f);
        result = lerp(grassColor, darkGrassColor, t);
    } else if (height < 10.0f) {
        // Mid elevation - grass to rock transition
        float t = (height - 5.0f) / 5.0f;
        result = lerp(darkGrassColor, rockColor, t);
    } else if (height < 15.0f) {
        // High elevation - rock
        float t = (height - 10.0f) / 5.0f;
        result = lerp(rockColor, darkRockColor, t);
    } else {
        // Snow caps
        float t = std::min(1.0f, (height - 15.0f) / 3.0f);
        result = lerp(darkRockColor, snowColor, t);
    }

    // Steep slopes → more rocky regardless of height
    if (slope > 1.0f && height > WATER_LEVEL + 0.5f) {
        float rockBlend = std::min(1.0f, (slope - 1.0f) / 2.0f);
        result = lerp(result, rockColor, rockBlend);
    }

    // Add slight noise variation to break up uniform colors
    float colorNoise = m_noise->noise2D(wx * 0.5f + 1000.0f, wz * 0.5f + 1000.0f);
    result.r += colorNoise * 0.03f;
    result.g += colorNoise * 0.025f;
    result.b += colorNoise * 0.02f;

    return result;
}

void Terrain::generateTerrain(Chunk& chunk,
                               std::deque<Mesh>& meshes,
                               std::vector<SceneObject>& objects) {
    float cx = chunk.coord.x;
    float cz = chunk.coord.z;
    float worldX0 = cx * CHUNK_SIZE;
    float worldZ0 = cz * CHUNK_SIZE;

    int res = Chunk::RESOLUTION;
    int stride = res + 1;

    // Sample heightmap
    for (int iz = 0; iz <= res; iz++) {
        for (int ix = 0; ix <= res; ix++) {
            float wx = worldX0 + (float)ix / res * CHUNK_SIZE;
            float wz = worldZ0 + (float)iz / res * CHUNK_SIZE;
            float h = rawTerrainHeight(wx, wz);
            chunk.heights[iz * stride + ix] = h;
        }
    }

    // Create terrain mesh for this chunk
    // We create one mesh per grid cell to support per-face coloring via materials
    // Actually, for efficiency, create one mesh but we'll use a single averaged color
    Mesh terrainMesh;
    int vertsPerSide = stride;

    for (int iz = 0; iz <= res; iz++) {
        for (int ix = 0; ix <= res; ix++) {
            float lx = (float)ix / res * CHUNK_SIZE;
            float lz = (float)iz / res * CHUNK_SIZE;
            float y = chunk.heights[iz * stride + ix];
            terrainMesh.vertices.push_back(Vec3(worldX0 + lx, y, worldZ0 + lz));
        }
    }

    // Triangulate the grid
    for (int iz = 0; iz < res; iz++) {
        for (int ix = 0; ix < res; ix++) {
            int tl = iz * vertsPerSide + ix;
            int tr = tl + 1;
            int bl = (iz + 1) * vertsPerSide + ix;
            int br = bl + 1;
            terrainMesh.faces.push_back({{{tl, bl, tr}}});
            terrainMesh.faces.push_back({{{tr, bl, br}}});
        }
    }

    meshes.push_back(std::move(terrainMesh));
    Mesh* meshPtr = &meshes.back();

    // Compute average height and slope for chunk color
    float avgHeight = 0.0f;
    float avgSlope = 0.0f;
    float centerX = worldX0 + CHUNK_SIZE * 0.5f;
    float centerZ = worldZ0 + CHUNK_SIZE * 0.5f;

    // Sample a few points for the average
    int samples = 0;
    for (int iz = 0; iz <= res; iz += 4) {
        for (int ix = 0; ix <= res; ix += 4) {
            float wx = worldX0 + (float)ix / res * CHUNK_SIZE;
            float wz = worldZ0 + (float)iz / res * CHUNK_SIZE;
            avgHeight += chunk.heights[iz * stride + ix];
            avgSlope += slopeAt(wx, wz);
            samples++;
        }
    }
    avgHeight /= samples;
    avgSlope /= samples;

    Color3 chunkColor = terrainColor(avgHeight, avgSlope, centerX, centerZ);
    Material terrainMat(chunkColor, 4.0f, 0.05f);

    objects.push_back({meshPtr, Mat4::identity(), terrainMat});
}

void Terrain::generateWater(const Chunk& chunk,
                             std::deque<Mesh>& meshes,
                             std::vector<SceneObject>& objects) {
    float cx = chunk.coord.x;
    float cz = chunk.coord.z;
    float worldX0 = cx * CHUNK_SIZE;
    float worldZ0 = cz * CHUNK_SIZE;

    int res = Chunk::RESOLUTION;
    int stride = res + 1;

    // Check if any vertex in this chunk is below water level
    bool hasWater = false;
    for (int i = 0; i < stride * stride; i++) {
        if (chunk.heights[i] < WATER_LEVEL) {
            hasWater = true;
            break;
        }
    }

    if (!hasWater) return;

    // Create a flat water plane mesh at WATER_LEVEL
    Mesh waterMesh;
    // Simple quad covering the chunk
    waterMesh.vertices.push_back(Vec3(worldX0, WATER_LEVEL, worldZ0));
    waterMesh.vertices.push_back(Vec3(worldX0 + CHUNK_SIZE, WATER_LEVEL, worldZ0));
    waterMesh.vertices.push_back(Vec3(worldX0, WATER_LEVEL, worldZ0 + CHUNK_SIZE));
    waterMesh.vertices.push_back(Vec3(worldX0 + CHUNK_SIZE, WATER_LEVEL, worldZ0 + CHUNK_SIZE));

    waterMesh.faces.push_back({{{0, 2, 1}}});
    waterMesh.faces.push_back({{{1, 2, 3}}});

    meshes.push_back(std::move(waterMesh));
    Mesh* meshPtr = &meshes.back();

    // Water material - dark blue with some shininess
    Material waterMat(Color3(0.10f, 0.22f, 0.42f), 32.0f, 0.6f);
    objects.push_back({meshPtr, Mat4::identity(), waterMat});
}
