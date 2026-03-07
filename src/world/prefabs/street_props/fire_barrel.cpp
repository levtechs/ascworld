#include "fire_barrel.h"
#include <cmath>

void placeFireBarrel(
    float x, float groundY, float z, float yawRad,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights,
    std::vector<AABB>& colliders
)
{
    Material barrelMat = pal.metal;
    barrelMat.color = Color3(0.18f, 0.12f, 0.08f);

    // --- Barrel body: cylinder radius=0.3, height=0.6 ---
    // createCylinder goes from Y=0 to Y=height, so translate to groundY
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY, z))
               * Mat4::rotateY(yawRad);
        objects.push_back(SceneObject{ meshCache.cylinder(0.3f, 0.6f, 12), t, barrelMat });
    }

    // --- Bottom rim ring ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY, z));
        objects.push_back(SceneObject{ meshCache.cylinder(0.32f, 0.06f, 12), t, barrelMat });
    }

    // --- Top rim ring (open top look) ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + 0.56f, z));
        objects.push_back(SceneObject{ meshCache.cylinder(0.32f, 0.04f, 12), t, barrelMat });
    }

    // --- Rust/grime band around middle ---
    {
        Material grimeMat = pal.grime;
        grimeMat.color = Color3(0.28f, 0.16f, 0.08f);
        Mat4 t = Mat4::translate(Vec3(x, groundY + 0.27f, z));
        objects.push_back(SceneObject{ meshCache.cylinder(0.31f, 0.06f, 12), t, grimeMat });
    }

    // --- Upper rust band ---
    {
        Material grimeMat = pal.grime;
        grimeMat.color = Color3(0.25f, 0.14f, 0.06f);
        Mat4 t = Mat4::translate(Vec3(x, groundY + 0.44f, z));
        objects.push_back(SceneObject{ meshCache.cylinder(0.31f, 0.04f, 12), t, grimeMat });
    }

    // --- Ventilation holes (small dark cubes punched through sides) ---
    for (int i = 0; i < 4; ++i)
    {
        float angle = yawRad + i * 1.5707963f; // 4 holes at 90-degree intervals
        float hx = x + 0.3f * std::cos(angle);
        float hz = z - 0.3f * std::sin(angle);
        Mat4 t = Mat4::translate(Vec3(hx, groundY + 0.25f, hz))
               * Mat4::rotateY(angle)
               * Mat4::scale(Vec3(0.06f, 0.08f, 0.06f));
        Material holeMat(Color3(0.05f, 0.03f, 0.02f), 4.0f, 0.0f);
        objects.push_back(SceneObject{ meshCache.cube(), t, holeMat });
    }

    // --- Ember glow cubes inside barrel (visible above rim) ---
    {
        Material emberMat(Color3(1.0f, 0.35f, 0.05f), 4.0f, 0.1f);
        Mat4 t = Mat4::translate(Vec3(x, groundY + 0.52f, z))
               * Mat4::scale(Vec3(0.2f, 0.08f, 0.2f));
        objects.push_back(SceneObject{ meshCache.cube(), t, emberMat });
    }
    {
        Material emberMat(Color3(0.9f, 0.25f, 0.03f), 4.0f, 0.1f);
        Mat4 t = Mat4::translate(Vec3(x + 0.08f, groundY + 0.55f, z - 0.06f))
               * Mat4::rotateY(0.7f)
               * Mat4::scale(Vec3(0.1f, 0.06f, 0.12f));
        objects.push_back(SceneObject{ meshCache.cube(), t, emberMat });
    }

    // --- Collider ---
    colliders.push_back(AABB(
        Vec3(x - 0.32f, groundY,       z - 0.32f),
        Vec3(x + 0.32f, groundY + 0.62f, z + 0.32f)
    ));

    // --- Point light: warm orange fire glow above barrel ---
    PointLight fire;
    fire.position  = Vec3(x, groundY + 0.9f, z);
    fire.color     = Color3(1.0f, 0.45f, 0.1f);
    fire.intensity = 2.0f;
    fire.radius    = 4.0f;
    lights.push_back(fire);

    // --- Secondary dimmer glow lower down (fire reflects off barrel) ---
    PointLight glow;
    glow.position  = Vec3(x, groundY + 0.5f, z);
    glow.color     = Color3(0.8f, 0.3f, 0.05f);
    glow.intensity = 0.6f;
    glow.radius    = 2.0f;
    lights.push_back(glow);
}
