#include "bench.h"

void placeBench(
    float x, float groundY, float z, float yawRad,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights,
    std::vector<AABB>& colliders
)
{
    // Bench: W=1.4, D=0.4, seat at Y=0.45, backrest ~0.6 tall from seat

    Mat4 base = Mat4::translate(Vec3(x, groundY, z)) * Mat4::rotateY(yawRad);

    // --- Seat slats (3 planks instead of a solid slab for realism) ---
    for (int i = 0; i < 3; ++i)
    {
        float zOff = -0.12f + i * 0.12f;  // spread across depth
        Mat4 local = Mat4::translate(Vec3(0.0f, 0.45f, zOff))
                   * Mat4::scale(Vec3(1.4f, 0.04f, 0.1f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.wallDark });
    }

    // --- Backrest slats (2 horizontal planks) ---
    {
        Mat4 local = Mat4::translate(Vec3(0.0f, 0.62f, -0.18f))
                   * Mat4::scale(Vec3(1.4f, 0.08f, 0.04f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.wallDark });
    }
    {
        Mat4 local = Mat4::translate(Vec3(0.0f, 0.82f, -0.18f))
                   * Mat4::scale(Vec3(1.4f, 0.08f, 0.04f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.wallDark });
    }

    // --- Left leg (A-frame style) ---
    {
        Mat4 local = Mat4::translate(Vec3(-0.58f, 0.225f, 0.0f))
                   * Mat4::scale(Vec3(0.06f, 0.45f, 0.35f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.metal });
    }

    // --- Center leg ---
    {
        Mat4 local = Mat4::translate(Vec3(0.0f, 0.225f, 0.0f))
                   * Mat4::scale(Vec3(0.05f, 0.45f, 0.3f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.metal });
    }

    // --- Right leg ---
    {
        Mat4 local = Mat4::translate(Vec3(0.58f, 0.225f, 0.0f))
                   * Mat4::scale(Vec3(0.06f, 0.45f, 0.35f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.metal });
    }

    // --- Left armrest ---
    {
        Mat4 local = Mat4::translate(Vec3(-0.62f, 0.55f, -0.04f))
                   * Mat4::scale(Vec3(0.06f, 0.16f, 0.3f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.metal });
    }
    // Left armrest top pad
    {
        Mat4 local = Mat4::translate(Vec3(-0.62f, 0.64f, -0.04f))
                   * Mat4::scale(Vec3(0.08f, 0.03f, 0.2f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.wallDark });
    }

    // --- Right armrest ---
    {
        Mat4 local = Mat4::translate(Vec3(0.62f, 0.55f, -0.04f))
                   * Mat4::scale(Vec3(0.06f, 0.16f, 0.3f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.metal });
    }
    // Right armrest top pad
    {
        Mat4 local = Mat4::translate(Vec3(0.62f, 0.64f, -0.04f))
                   * Mat4::scale(Vec3(0.08f, 0.03f, 0.2f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.wallDark });
    }

    // --- Stretcher bar (horizontal brace between legs under seat) ---
    {
        Mat4 local = Mat4::translate(Vec3(0.0f, 0.12f, 0.0f))
                   * Mat4::scale(Vec3(1.2f, 0.04f, 0.04f));
        objects.push_back(SceneObject{ meshCache.cube(), base * local, pal.metal });
    }

    // --- Collider: covers seat + backrest + armrests ---
    float hw = 0.72f;
    float hd = 0.22f;
    colliders.push_back(AABB(
        Vec3(x - hw, groundY,        z - hd - 0.03f),
        Vec3(x + hw, groundY + 1.05f, z + hd)
    ));

    (void)lights;
}
