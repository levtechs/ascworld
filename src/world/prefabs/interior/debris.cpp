#include "world/prefabs/interior/debris.h"
#include "core/vec_math.h"

void placeDebris(float x, float groundY, float z, uint32_t seed, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects)
{
    // LCG helper — advances seed and returns [0, 1)
    auto lcg = [](uint32_t& s) -> float {
        s = s * 1664525u + 1013904223u;
        return static_cast<float>(s >> 16) / 65535.0f;
    };

    // 3 to 5 pieces determined by seed
    uint32_t s = seed;
    int count = 3 + static_cast<int>(lcg(s) * 3.0f); // [3, 5]

    const float spread = 0.8f; // scatter radius around center

    for (int i = 0; i < count; ++i)
    {
        float rx   = (lcg(s) - 0.5f) * 2.0f * spread;  // [-spread, +spread]
        float rz   = (lcg(s) - 0.5f) * 2.0f * spread;
        float sc   = 0.2f + lcg(s) * 0.3f;              // [0.2, 0.5]
        float yaw  = lcg(s) * 6.2832f;                  // random rotation

        // Sits on ground; half-height = sc*0.5
        float py = groundY + sc * 0.5f;

        Mat4 transform = Mat4::translate({x + rx, py, z + rz})
                       * Mat4::rotateY(yaw)
                       * Mat4::scale({sc, sc, sc});
        objects.push_back({meshCache.cube(), transform, pal.grime});
    }
}
