#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <algorithm>

// Chunk coordinate pair
struct ChunkCoord {
    int x, z;
    bool operator==(const ChunkCoord& o) const { return x == o.x && z == o.z; }
};

// Hash for ChunkCoord
struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        size_t h = std::hash<int>()(c.x);
        h ^= std::hash<int>()(c.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// A chunk represents a CHUNK_SIZE x CHUNK_SIZE area of the world
struct Chunk {
    ChunkCoord coord;

    // Indices into World's global vectors for objects/colliders in this chunk
    size_t objectStart, objectCount;
    size_t colliderStart, colliderCount;
    size_t lightStart, lightCount;

    // Heightmap: (RESOLUTION+1)^2 heights
    static constexpr int RESOLUTION = 16; // samples per chunk edge
    float heights[(RESOLUTION + 1) * (RESOLUTION + 1)];

    // Bilinear interpolation of heightmap at local coordinates [0, chunkSize]
    float heightAt(float localX, float localZ, float chunkSize) const {
        float gx = (localX / chunkSize) * RESOLUTION;
        float gz = (localZ / chunkSize) * RESOLUTION;

        gx = std::max(0.0f, std::min((float)RESOLUTION, gx));
        gz = std::max(0.0f, std::min((float)RESOLUTION, gz));

        int ix = (int)gx;
        int iz = (int)gz;
        if (ix >= RESOLUTION) ix = RESOLUTION - 1;
        if (iz >= RESOLUTION) iz = RESOLUTION - 1;

        float fx = gx - ix;
        float fz = gz - iz;

        int stride = RESOLUTION + 1;
        float h00 = heights[iz * stride + ix];
        float h10 = heights[iz * stride + ix + 1];
        float h01 = heights[(iz + 1) * stride + ix];
        float h11 = heights[(iz + 1) * stride + ix + 1];

        float h0 = h00 + fx * (h10 - h00);
        float h1 = h01 + fx * (h11 - h01);
        return h0 + fz * (h1 - h0);
    }
};

// Deterministic random number generation from chunk coordinates + seed
inline uint32_t chunkRandom(uint32_t seed, int cx, int cz, int index) {
    uint32_t h = seed;
    h ^= (uint32_t)(cx + 1000) * 374761393u;
    h ^= (uint32_t)(cz + 1000) * 668265263u;
    h ^= (uint32_t)(index) * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return h;
}

inline float chunkRandomFloat(uint32_t seed, int cx, int cz, int index) {
    return (float)(chunkRandom(seed, cx, cz, index) & 0xFFFF) / 65535.0f;
}
