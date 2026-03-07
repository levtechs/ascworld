#include "bollard.h"

void placeBollard(
    float x, float groundY, float z, float yawRad,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights,
    std::vector<AABB>& colliders
)
{
    // --- Post: cylinder radius=0.12, height=0.9 ---
    // createCylinder goes from Y=0 to Y=height, so translate to groundY
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY, z))
               * Mat4::rotateY(yawRad);
        objects.push_back(SceneObject{ meshCache.cylinder(0.12f, 0.9f, 8), t, pal.metal });
    }

    // --- Reflective cap on top: small sphere ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + 0.9f, z));
        Material capMat = pal.accent;
        capMat.shininess = 64.0f;
        capMat.specular  = 0.8f;
        objects.push_back(SceneObject{ meshCache.sphere(4, 8, 0.13f), t, capMat });
    }

    // --- Reflective stripe band ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + 0.65f, z));
        objects.push_back(SceneObject{ meshCache.cylinder(0.125f, 0.05f, 8), t, pal.accent });
    }

    // --- Collider ---
    colliders.push_back(AABB(
        Vec3(x - 0.12f, groundY,       z - 0.12f),
        Vec3(x + 0.12f, groundY + 0.9f, z + 0.12f)
    ));

    (void)lights;
}
