#include "world/prefabs/interior/table.h"
#include "core/vec_math.h"
#include "rendering/framebuffer.h"

void placeTable(float x, float groundY, float z, float yawRad, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects, std::vector<AABB>& colliders)
{
    // Tint floor color for wood-like table surface
    Material tableMat = pal.floor;
    tableMat.color.r = pal.floor.color.r * 0.85f;
    tableMat.color.g = pal.floor.color.g * 0.80f;
    tableMat.color.b = pal.floor.color.b * 0.75f;

    Mat4 base = Mat4::translate({x, groundY, z}) * Mat4::rotateY(yawRad);

    // --- Tabletop: 1.2 wide x 0.05 tall x 0.6 deep ---
    {
        Mat4 local = Mat4::translate({0.0f, 0.725f, 0.0f}) * Mat4::scale({1.2f, 0.05f, 0.6f});
        objects.push_back({meshCache.cube(), base * local, tableMat});
    }

    // --- Edge trim (slightly darker band around tabletop perimeter) ---
    {
        Material edgeMat = tableMat;
        edgeMat.color.r *= 0.7f; edgeMat.color.g *= 0.7f; edgeMat.color.b *= 0.7f;
        // Front edge
        Mat4 front = Mat4::translate({0.0f, 0.725f, 0.29f}) * Mat4::scale({1.2f, 0.05f, 0.02f});
        objects.push_back({meshCache.cube(), base * front, edgeMat});
        // Back edge
        Mat4 back = Mat4::translate({0.0f, 0.725f, -0.29f}) * Mat4::scale({1.2f, 0.05f, 0.02f});
        objects.push_back({meshCache.cube(), base * back, edgeMat});
        // Left edge
        Mat4 left = Mat4::translate({-0.59f, 0.725f, 0.0f}) * Mat4::scale({0.02f, 0.05f, 0.6f});
        objects.push_back({meshCache.cube(), base * left, edgeMat});
        // Right edge
        Mat4 right = Mat4::translate({0.59f, 0.725f, 0.0f}) * Mat4::scale({0.02f, 0.05f, 0.6f});
        objects.push_back({meshCache.cube(), base * right, edgeMat});
    }

    // --- 4 legs ---
    const float lx = 0.50f;
    const float lz = 0.25f;
    const float inset = 0.08f;
    const float legH = 0.725f;
    const float legR = 0.04f;

    float legPositions[4][2] = {
        { lx - inset,  lz - inset},
        {-lx + inset,  lz - inset},
        { lx - inset, -lz + inset},
        {-lx + inset, -lz + inset},
    };

    for (int i = 0; i < 4; ++i)
    {
        // createCylinder goes from Y=0 to Y=height, so translate to Y=0 (relative to base)
        Mat4 local = Mat4::translate({legPositions[i][0], 0.0f, legPositions[i][1]});
        objects.push_back({meshCache.cylinder(legR, legH), base * local, pal.metal});
    }

    // --- Cross-braces between legs (H-frame under tabletop) ---
    {
        // Long brace along X (front pair)
        Mat4 longFront = Mat4::translate({0.0f, 0.3f, lz - inset})
                       * Mat4::scale({0.84f, 0.03f, 0.03f});
        objects.push_back({meshCache.cube(), base * longFront, pal.metal});

        // Long brace along X (back pair)
        Mat4 longBack = Mat4::translate({0.0f, 0.3f, -(lz - inset)})
                      * Mat4::scale({0.84f, 0.03f, 0.03f});
        objects.push_back({meshCache.cube(), base * longBack, pal.metal});

        // Short brace along Z (center cross)
        Mat4 shortCenter = Mat4::translate({0.0f, 0.3f, 0.0f})
                         * Mat4::scale({0.03f, 0.03f, 0.34f});
        objects.push_back({meshCache.cube(), base * shortCenter, pal.metal});
    }

    // --- Foot pads (small flat discs at leg bottoms) ---
    for (int i = 0; i < 4; ++i)
    {
        Mat4 local = Mat4::translate({legPositions[i][0], 0.0f, legPositions[i][1]});
        objects.push_back({meshCache.cylinder(0.06f, 0.02f, 6), base * local, pal.metal});
    }

    // AABB
    colliders.push_back(AABB(
        {x - 0.6f, groundY,        z - 0.3f},
        {x + 0.6f, groundY + 0.75f, z + 0.3f}
    ));
}
