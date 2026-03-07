#include "streetlamp.h"
#include <cmath>

void placeStreetlamp(
    float x, float groundY, float z, float yawRad,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights,
    std::vector<AABB>& colliders
)
{
    float cosY = std::cos(yawRad);
    float sinY = std::sin(yawRad);

    // --- Base plate (square footing on ground) ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + 0.04f, z))
               * Mat4::rotateY(yawRad)
               * Mat4::scale(Vec3(0.5f, 0.08f, 0.5f));
        objects.push_back(SceneObject{ meshCache.cube(), t, pal.wallDark });
    }

    // --- Post (main vertical pole) ---
    // Cylinder: radius=0.12, height=4.0  (slightly thinner than before for elegance)
    // Note: createCylinder goes from Y=0 to Y=height, so translate to groundY
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY, z))
               * Mat4::rotateY(yawRad);
        objects.push_back(SceneObject{ meshCache.cylinder(0.12f, 4.0f, 10), t, pal.metal });
    }

    // --- Decorative collar ring at arm junction ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + 3.88f, z));
        objects.push_back(SceneObject{ meshCache.cylinder(0.18f, 0.08f, 10), t, pal.trim });
    }

    // --- Curved neck (short angled segment from post to arm) ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + 4.0f, z))
               * Mat4::rotateY(yawRad)
               * Mat4::translate(Vec3(0.12f, 0.06f, 0.0f))
               * Mat4::scale(Vec3(0.24f, 0.06f, 0.06f));
        objects.push_back(SceneObject{ meshCache.cube(), t, pal.metal });
    }

    // --- Arm (horizontal bar extending from top of post) ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + 4.06f, z))
               * Mat4::rotateY(yawRad)
               * Mat4::translate(Vec3(0.55f, 0.0f, 0.0f))
               * Mat4::scale(Vec3(0.7f, 0.06f, 0.06f));
        objects.push_back(SceneObject{ meshCache.cube(), t, pal.metal });
    }

    // --- Lamp housing (wider box at end of arm) ---
    float armEndX = x + 0.9f * cosY;
    float armEndZ = z - 0.9f * sinY;
    {
        Mat4 t = Mat4::translate(Vec3(armEndX, groundY + 3.96f, armEndZ))
               * Mat4::rotateY(yawRad)
               * Mat4::scale(Vec3(0.28f, 0.12f, 0.22f));
        objects.push_back(SceneObject{ meshCache.cube(), t, pal.trim });
    }

    // --- Lamp diffuser (glowing panel underneath housing) ---
    {
        Material diffuserMat(Color3(0.95f, 0.97f, 1.0f), 4.0f, 0.1f);
        Mat4 t = Mat4::translate(Vec3(armEndX, groundY + 3.88f, armEndZ))
               * Mat4::rotateY(yawRad)
               * Mat4::scale(Vec3(0.22f, 0.03f, 0.16f));
        objects.push_back(SceneObject{ meshCache.cube(), t, diffuserMat });
    }

    // --- Second arm + lamp on opposite side (for major intersections look) ---
    float arm2X = x - 0.7f * cosY;
    float arm2Z = z + 0.7f * sinY;
    {
        // Shorter secondary arm
        Mat4 t = Mat4::translate(Vec3(x, groundY + 4.06f, z))
               * Mat4::rotateY(yawRad)
               * Mat4::translate(Vec3(-0.4f, 0.0f, 0.0f))
               * Mat4::scale(Vec3(0.5f, 0.05f, 0.05f));
        objects.push_back(SceneObject{ meshCache.cube(), t, pal.metal });
    }
    // Secondary lamp housing (smaller)
    {
        Mat4 t = Mat4::translate(Vec3(arm2X, groundY + 3.98f, arm2Z))
               * Mat4::rotateY(yawRad)
               * Mat4::scale(Vec3(0.2f, 0.1f, 0.16f));
        objects.push_back(SceneObject{ meshCache.cube(), t, pal.trim });
    }
    // Secondary diffuser
    {
        Material diffuserMat(Color3(0.90f, 0.93f, 1.0f), 4.0f, 0.1f);
        Mat4 t = Mat4::translate(Vec3(arm2X, groundY + 3.91f, arm2Z))
               * Mat4::rotateY(yawRad)
               * Mat4::scale(Vec3(0.16f, 0.02f, 0.12f));
        objects.push_back(SceneObject{ meshCache.cube(), t, diffuserMat });
    }

    // --- Collider for post ---
    colliders.push_back(AABB(
        Vec3(x - 0.15f, groundY,        z - 0.15f),
        Vec3(x + 0.15f, groundY + 4.0f, z + 0.15f)
    ));

    // --- Primary point light ---
    PointLight lamp;
    lamp.position  = Vec3(armEndX, groundY + 3.8f, armEndZ);
    lamp.color     = Color3(0.85f, 0.92f, 1.0f);
    lamp.intensity = 1.5f;
    lamp.radius    = 8.0f;
    lights.push_back(lamp);

    // --- Secondary point light (dimmer) ---
    PointLight lamp2;
    lamp2.position  = Vec3(arm2X, groundY + 3.85f, arm2Z);
    lamp2.color     = Color3(0.80f, 0.88f, 0.96f);
    lamp2.intensity = 0.8f;
    lamp2.radius    = 5.0f;
    lights.push_back(lamp2);
}
