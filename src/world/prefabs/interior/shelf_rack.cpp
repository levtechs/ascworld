#include "world/prefabs/interior/shelf_rack.h"
#include "core/vec_math.h"

void placeShelfRack(float x, float groundY, float z, float yawRad, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects, std::vector<AABB>& colliders)
{
    // Rack dims: 0.8 wide (X) x 2.0 tall (Y) x 0.3 deep (Z)
    const float rackW  = 0.8f;
    const float rackH  = 2.0f;
    const float rackD  = 0.3f;
    const float postR  = 0.03f;
    const float shelfT = 0.04f;
    const float halfW  = rackW * 0.5f;
    const float halfD  = rackD * 0.5f;

    Mat4 base = Mat4::translate({x, groundY, z}) * Mat4::rotateY(yawRad);

    // --- 4 vertical corner posts ---
    float postXs[2] = { halfW - postR,  -(halfW - postR) };
    float postZs[2] = { halfD - postR,  -(halfD - postR) };

    for (int xi = 0; xi < 2; ++xi)
    {
        for (int zi = 0; zi < 2; ++zi)
        {
            // createCylinder goes from Y=0 to Y=height, so translate to Y=0
            Mat4 local = Mat4::translate({postXs[xi], 0.0f, postZs[zi]});
            objects.push_back({meshCache.cylinder(postR, rackH), base * local, pal.metal});
        }
    }

    // --- 3 horizontal shelves ---
    float shelfYs[3] = { 0.55f, 1.10f, 1.65f };
    for (int i = 0; i < 3; ++i)
    {
        Mat4 local = Mat4::translate({0.0f, shelfYs[i], 0.0f})
                   * Mat4::scale({rackW, shelfT, rackD});
        objects.push_back({meshCache.cube(), base * local, pal.metal});
    }

    // --- Diagonal cross-brace on back (X pattern for structural detail) ---
    {
        // Brace 1: bottom-left to top-right (on back face)
        // Approximate with a thin rotated cube
        float braceLen = 1.6f;  // approximate diagonal
        float braceAngle = 0.85f;  // atan2(rackH*0.8, rackW)
        Mat4 brace1 = Mat4::translate({0.0f, rackH * 0.5f, -(halfD - 0.01f)})
                    * Mat4::rotateZ(braceAngle)
                    * Mat4::scale({0.03f, braceLen, 0.02f});
        objects.push_back({meshCache.cube(), base * brace1, pal.metal});

        // Brace 2: other diagonal
        Mat4 brace2 = Mat4::translate({0.0f, rackH * 0.5f, -(halfD - 0.01f)})
                    * Mat4::rotateZ(-braceAngle)
                    * Mat4::scale({0.03f, braceLen, 0.02f});
        objects.push_back({meshCache.cube(), base * brace2, pal.metal});
    }

    // --- Items on shelves (small boxes and cylinders for visual interest) ---
    // Shelf 1 (bottom): 2 small boxes
    {
        Material boxMat = pal.wallDark;
        Mat4 box1 = Mat4::translate({-0.2f, shelfYs[0] + shelfT * 0.5f + 0.06f, 0.0f})
                  * Mat4::scale({0.15f, 0.12f, 0.12f});
        objects.push_back({meshCache.cube(), base * box1, boxMat});

        Material boxMat2 = pal.accentDim;
        Mat4 box2 = Mat4::translate({0.15f, shelfYs[0] + shelfT * 0.5f + 0.05f, 0.02f})
                  * Mat4::rotateY(0.3f)
                  * Mat4::scale({0.12f, 0.1f, 0.1f});
        objects.push_back({meshCache.cube(), base * box2, boxMat2});
    }

    // Shelf 2 (middle): a cylinder (canister) + small box
    {
        // createCylinder Y=0 to Y=height; place bottom on shelf surface
        Mat4 can = Mat4::translate({0.1f, shelfYs[1] + shelfT * 0.5f, 0.0f});
        objects.push_back({meshCache.cylinder(0.05f, 0.16f, 8), base * can, pal.trim});

        Material crateMat = pal.grime;
        Mat4 box = Mat4::translate({-0.18f, shelfYs[1] + shelfT * 0.5f + 0.07f, -0.02f})
                 * Mat4::scale({0.18f, 0.14f, 0.14f});
        objects.push_back({meshCache.cube(), base * box, crateMat});
    }

    // Shelf 3 (top): a tall cylinder + small item
    {
        // createCylinder Y=0 to Y=height; place bottom on shelf surface
        Mat4 bottle = Mat4::translate({-0.1f, shelfYs[2] + shelfT * 0.5f, 0.0f});
        objects.push_back({meshCache.cylinder(0.03f, 0.2f, 6), base * bottle, pal.accent});

        Mat4 item = Mat4::translate({0.2f, shelfYs[2] + shelfT * 0.5f + 0.04f, 0.0f})
                  * Mat4::scale({0.1f, 0.08f, 0.08f});
        objects.push_back({meshCache.cube(), base * item, pal.wallDark});
    }

    // --- AABB ---
    colliders.push_back(AABB(
        {x - halfW, groundY,         z - halfD},
        {x + halfW, groundY + rackH, z + halfD}
    ));
}
