#include "world/prefabs/structural/door_frame.h"
#include "core/vec_math.h"

void placeDoorFrame(float x, float groundY, float z, float yawRad,
    float wallWidth, float wallHeight, float wallDepth, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects, std::vector<AABB>& colliders)
{
    // Door opening dimensions — match building constants
    const float doorW  = 1.6f;
    const float doorH  = 2.4f;
    const float halfDoor = doorW * 0.5f;

    // Width of solid wall on each side of the door opening
    float sideW = (wallWidth - doorW) * 0.5f;

    Mat4 base = Mat4::translate({x, groundY, z}) * Mat4::rotateY(yawRad);

    // --- Left post (local -X side) ---
    if (sideW > 0.001f)
    {
        float postCenterX = -(halfDoor + sideW * 0.5f);
        Mat4 local = Mat4::translate({postCenterX, wallHeight * 0.5f, 0.0f})
                   * Mat4::scale({sideW, wallHeight, wallDepth});
        objects.push_back({meshCache.cube(), base * local, pal.wall});
    }

    // --- Right post (local +X side) ---
    if (sideW > 0.001f)
    {
        float postCenterX = halfDoor + sideW * 0.5f;
        Mat4 local = Mat4::translate({postCenterX, wallHeight * 0.5f, 0.0f})
                   * Mat4::scale({sideW, wallHeight, wallDepth});
        objects.push_back({meshCache.cube(), base * local, pal.wall});
    }

    // --- Lintel above the door opening ---
    {
        float lintelH  = wallHeight - doorH;
        float lintelCY = doorH + lintelH * 0.5f;
        if (lintelH > 0.001f)
        {
            Mat4 local = Mat4::translate({0.0f, lintelCY, 0.0f})
                       * Mat4::scale({doorW, lintelH, wallDepth});
            objects.push_back({meshCache.cube(), base * local, pal.wall});
        }
    }

    // Door frame is visual trim only; wall colliders already define passability.
    (void)colliders;
}
