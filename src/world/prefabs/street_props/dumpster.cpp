#include "dumpster.h"
#include <cmath>

void placeDumpster(
    float x, float groundY, float z, float yawRad,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights,
    std::vector<AABB>& colliders
)
{
    // Dimensions: W=1.2, D=0.6, H=0.8
    const float W = 1.2f;
    const float D = 0.6f;
    const float H = 0.8f;

    Mat4 base = Mat4::translate(Vec3(x, groundY, z)) * Mat4::rotateY(yawRad);

    // --- Main body ---
    {
        Mat4 local = Mat4::translate(Vec3(0.0f, H * 0.5f, 0.0f))
                   * Mat4::scale(Vec3(W, H, D));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.metal });
    }

    // --- Lid: thin slab on top, slightly ajar (rotated a bit) ---
    {
        Mat4 local = Mat4::translate(Vec3(0.0f, H + 0.03f, -0.02f))
                   * Mat4::rotateX(0.08f)  // slightly open
                   * Mat4::scale(Vec3(W + 0.04f, 0.06f, D + 0.04f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.wallDark });
    }

    // --- Lid hinge bar (cylinder along back edge) ---
    {
        Mat4 local = Mat4::translate(Vec3(0.0f, H, -D * 0.5f))
                   * Mat4::rotateZ(1.5707963f)  // lay cylinder along X
                   * Mat4::scale(Vec3(1.0f, 1.0f, 1.0f));
        objects.push_back(SceneObject{ meshCache.cylinder(0.02f, W * 0.9f, 6), base * local, pal.metal });
    }

    // --- Bottom trim rail ---
    {
        Mat4 local = Mat4::translate(Vec3(0.0f, 0.04f, 0.0f))
                   * Mat4::scale(Vec3(W + 0.02f, 0.06f, D + 0.02f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.accentDim });
    }

    // --- Top trim rail ---
    {
        Mat4 local = Mat4::translate(Vec3(0.0f, H - 0.02f, 0.0f))
                   * Mat4::scale(Vec3(W + 0.01f, 0.04f, D + 0.01f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.accentDim });
    }

    // --- Front handles (2 small cubes) ---
    {
        Mat4 local = Mat4::translate(Vec3(-0.35f, H * 0.55f, D * 0.5f + 0.04f))
                   * Mat4::scale(Vec3(0.12f, 0.06f, 0.06f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.metal });
    }
    {
        Mat4 local = Mat4::translate(Vec3(0.35f, H * 0.55f, D * 0.5f + 0.04f))
                   * Mat4::scale(Vec3(0.12f, 0.06f, 0.06f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.metal });
    }

    // --- Wheels (4 small cylinders at bottom corners) ---
    const float wheelR = 0.06f;
    const float wheelT = 0.04f;
    float wx = W * 0.5f - 0.1f;
    float wz = D * 0.5f - 0.08f;
    for (int si = -1; si <= 1; si += 2)
    {
        for (int fi = -1; fi <= 1; fi += 2)
        {
            Mat4 local = Mat4::translate(Vec3(si * wx, wheelR, fi * wz))
                       * Mat4::rotateX(1.5707963f);  // lay on side
            objects.push_back(SceneObject{ meshCache.cylinder(wheelR, wheelT, 8), base * local, pal.grime });
        }
    }

    // --- Rust patches (darker splotches on body) ---
    {
        Material rustMat = pal.grime;
        rustMat.color = Color3(0.22f, 0.12f, 0.06f);
        Mat4 local = Mat4::translate(Vec3(0.2f, H * 0.35f, D * 0.5f + 0.005f))
                   * Mat4::scale(Vec3(0.25f, 0.18f, 0.01f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, rustMat });
    }
    {
        Material rustMat = pal.grime;
        rustMat.color = Color3(0.2f, 0.1f, 0.05f);
        Mat4 local = Mat4::translate(Vec3(-0.3f, H * 0.6f, -D * 0.5f - 0.005f))
                   * Mat4::scale(Vec3(0.2f, 0.15f, 0.01f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, rustMat });
    }

    // --- Collider ---
    float extent = (W > D ? W : D) * 0.5f + 0.05f;
    colliders.push_back(AABB(
        Vec3(x - extent, groundY,        z - extent),
        Vec3(x + extent, groundY + H + 0.09f, z + extent)
    ));

    (void)lights;
}
