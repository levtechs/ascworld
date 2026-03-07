#include "world/prefabs/structural/rubble_pile.h"
#include "core/vec_math.h"

void placeRubblePile(float x, float groundY, float z, uint32_t seed, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects, std::vector<AABB>& colliders)
{
    // LCG helper — advances seed and returns [0, 1)
    auto lcg = [](uint32_t& s) -> float {
        s = s * 1664525u + 1013904223u;
        return static_cast<float>(s >> 16) / 65535.0f;
    };

    uint32_t s = seed;

    // 5 to 8 chunks
    int count = 5 + static_cast<int>(lcg(s) * 4.0f); // [5, 8]

    // Track footprint extents to build the AABB
    float minX = x, maxX = x;
    float minZ = z, maxZ = z;
    float maxY = groundY;

    // Pile spread radius — chunks cluster toward center in a mound shape
    const float spread = 0.7f;

    for (int i = 0; i < count; ++i)
    {
        // Mound: pieces near center are higher (mound factor decreases with distance from center)
        float rx  = (lcg(s) - 0.5f) * 2.0f * spread;
        float rz  = (lcg(s) - 0.5f) * 2.0f * spread;
        float sc  = 0.3f + lcg(s) * 0.5f;  // [0.3, 0.8]
        float yaw = lcg(s) * 6.2832f;

        // Mound height: center pieces sit higher
        float distFrac = (rx * rx + rz * rz) / (spread * spread); // 0 = center, 1 = edge
        float heapY    = groundY + sc * 0.25f + (1.0f - distFrac) * 0.25f;

        // Alternate materials between grime and wallDark for variety
        const Material& mat = (i % 2 == 0) ? pal.grime : pal.wallDark;

        Mat4 transform = Mat4::translate({x + rx, heapY, z + rz})
                       * Mat4::rotateY(yaw)
                       * Mat4::scale({sc, sc * 0.6f, sc}); // flatten slightly to look like rubble
        objects.push_back({meshCache.cube(), transform, mat});

        // Grow footprint extents
        float halfSc = sc * 0.5f;
        if (x + rx - halfSc < minX) minX = x + rx - halfSc;
        if (x + rx + halfSc > maxX) maxX = x + rx + halfSc;
        if (z + rz - halfSc < minZ) minZ = z + rz - halfSc;
        if (z + rz + halfSc > maxZ) maxZ = z + rz + halfSc;
        float topY = heapY + sc * 0.6f * 0.5f;
        if (topY > maxY) maxY = topY;
    }

    // Single AABB covering the entire pile footprint
    colliders.push_back(AABB(
        {minX, groundY, minZ},
        {maxX, maxY,    maxZ}
    ));
}
