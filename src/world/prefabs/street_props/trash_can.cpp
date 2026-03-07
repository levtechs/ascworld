#include "trash_can.h"
#include <cmath>

void placeTrashCan(
    float x, float groundY, float z, float yawRad,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights,
    std::vector<AABB>& colliders
)
{
    // --- Foot ring (wider base for stability) ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY, z));
        objects.push_back(SceneObject{ meshCache.cylinder(0.28f, 0.06f, 10), t, pal.metal });
    }

    // --- Body: cylinder radius=0.25, height=0.7 ---
    // createCylinder goes from Y=0 to Y=height, so translate to groundY
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY, z))
               * Mat4::rotateY(yawRad);
        objects.push_back(SceneObject{ meshCache.cylinder(0.25f, 0.7f, 10), t, pal.grime });
    }

    // --- Rim band (decorative ring near top) ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + 0.62f, z));
        objects.push_back(SceneObject{ meshCache.cylinder(0.26f, 0.04f, 10), t, pal.metal });
    }

    // --- Lid: thin disc on top ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + 0.7f, z));
        objects.push_back(SceneObject{ meshCache.cylinder(0.27f, 0.05f, 10), t, pal.metal });
    }

    // --- Lid handle (small cube on top of lid) ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + 0.77f, z))
               * Mat4::rotateY(yawRad)
               * Mat4::scale(Vec3(0.12f, 0.05f, 0.04f));
        objects.push_back(SceneObject{ meshCache.cube(), t, pal.metal });
    }

    // --- Lower body band (second decorative ring) ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + 0.18f, z));
        objects.push_back(SceneObject{ meshCache.cylinder(0.26f, 0.03f, 10), t, pal.wallDark });
    }

    // --- Collider ---
    colliders.push_back(AABB(
        Vec3(x - 0.27f, groundY,        z - 0.27f),
        Vec3(x + 0.27f, groundY + 0.8f, z + 0.27f)
    ));

    (void)lights;
}
