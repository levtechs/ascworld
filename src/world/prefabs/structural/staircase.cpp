#include "world/prefabs/structural/staircase.h"
#include "core/vec_math.h"

void placeStaircase(float x, float groundY, float z, float width, float totalHeight, float depth,
    int axis, bool ascending, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects,
    std::vector<AABB>& colliders,
    std::vector<SlopeCollider>& slopeColliders)
{
    const int numSteps = 9;

    float stepH = totalHeight / numSteps;
    float stepD = depth / numSteps;

    for (int i = 0; i < numSteps; ++i)
    {
        float travelOffset = (i + 0.5f) * stepD;
        float heightCenter = groundY + (i + 0.5f) * stepH;

        float sign = ascending ? 1.0f : -1.0f;

        float lx, lz, sx, sz;
        if (axis == 0)
        {
            lx = x + sign * travelOffset;
            lz = z;
            sx = stepD;
            sz = width;
        }
        else
        {
            lx = x;
            lz = z + sign * travelOffset;
            sx = width;
            sz = stepD;
        }

        Mat4 transform = Mat4::translate({lx, heightCenter, lz})
                       * Mat4::scale({sx, stepH, sz});
        const Material& stepMat = (i % 2 == 0) ? pal.trim : pal.wallDark;
        objects.push_back({meshCache.cube(), transform, stepMat});

        // AABB collider for each step so the player can walk on them
        float halfSx = sx * 0.5f;
        float halfSz = sz * 0.5f;
        float stepBottom = groundY + i * stepH;
        float stepTop    = groundY + (i + 1) * stepH;
        colliders.push_back(AABB(
            {lx - halfSx, stepBottom, lz - halfSz},
            {lx + halfSx, stepTop,    lz + halfSz}
        ));
    }

    (void)slopeColliders;
}
