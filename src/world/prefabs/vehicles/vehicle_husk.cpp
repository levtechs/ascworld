#include "world/prefabs/vehicles/vehicle_husk.h"
#include <cmath>

// Dimensions (L x W x H): ~3.5 x 1.6 x 1.4
static const float kBodyL    = 3.5f;
static const float kBodyW    = 1.6f;
static const float kBodyH    = 0.85f;
static const float kRoofL    = 2.0f;
static const float kRoofW    = 1.4f;
static const float kRoofH    = 0.55f;
static const float kWheelR   = 0.32f;
static const float kWheelT   = 0.22f;
static const float kHoodL    = 0.8f;
static const float kHoodH    = 0.45f;

void placeVehicleHusk(
    float x, float groundY, float z, float yawRad,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<AABB>& colliders)
{
    Material bodyMat  = pal.metal;
    Material rimMat   = pal.grime;
    Material roofMat  = pal.metal;
    Material glassMat(Color3(0.15f, 0.2f, 0.25f), 64.0f, 0.7f);  // dark tinted glass
    Material trimMat  = pal.trim;

    Mat4 root = Mat4::translate({ x, groundY, z }) * Mat4::rotateY(yawRad);

    // ---- Body (main box) ----
    {
        float cy = kBodyH * 0.5f;
        Mat4 t = root * Mat4::translate({ 0.f, cy, 0.f })
               * Mat4::scale({ kBodyL, kBodyH, kBodyW });
        objects.push_back({ meshCache.cube(), t, bodyMat });
    }

    // ---- Door line groove (left side) ----
    {
        Material grooveMat = pal.metal;
        grooveMat.color.r *= 0.5f; grooveMat.color.g *= 0.5f; grooveMat.color.b *= 0.5f;
        Mat4 t = root * Mat4::translate({ 0.1f, kBodyH * 0.5f, kBodyW * 0.5f + 0.005f })
               * Mat4::scale({ 0.02f, kBodyH * 0.7f, 0.01f });
        objects.push_back({ meshCache.cube(), t, grooveMat });
    }
    // ---- Door line groove (right side) ----
    {
        Material grooveMat = pal.metal;
        grooveMat.color.r *= 0.5f; grooveMat.color.g *= 0.5f; grooveMat.color.b *= 0.5f;
        Mat4 t = root * Mat4::translate({ 0.1f, kBodyH * 0.5f, -kBodyW * 0.5f - 0.005f })
               * Mat4::scale({ 0.02f, kBodyH * 0.7f, 0.01f });
        objects.push_back({ meshCache.cube(), t, grooveMat });
    }

    // ---- Roof ----
    {
        float cy = kBodyH + kRoofH * 0.5f;
        Mat4 t = root * Mat4::translate({ -0.1f, cy, 0.f })
               * Mat4::scale({ kRoofL, kRoofH, kRoofW });
        objects.push_back({ meshCache.cube(), t, roofMat });
    }

    // ---- Windshield (front glass, angled) ----
    {
        float glassY = kBodyH + kRoofH * 0.35f;
        float glassX = -0.1f + kRoofL * 0.5f + 0.02f;
        Mat4 t = root * Mat4::translate({ glassX, glassY, 0.f })
               * Mat4::rotateZ(0.25f)  // slight tilt
               * Mat4::scale({ 0.06f, kRoofH * 0.7f, kRoofW - 0.1f });
        objects.push_back({ meshCache.cube(), t, glassMat });
    }

    // ---- Rear window ----
    {
        float glassY = kBodyH + kRoofH * 0.35f;
        float glassX = -0.1f - kRoofL * 0.5f - 0.02f;
        Mat4 t = root * Mat4::translate({ glassX, glassY, 0.f })
               * Mat4::rotateZ(-0.2f)
               * Mat4::scale({ 0.05f, kRoofH * 0.6f, kRoofW - 0.15f });
        objects.push_back({ meshCache.cube(), t, glassMat });
    }

    // ---- Hood ----
    {
        float frontX = kBodyL * 0.5f - kHoodL * 0.5f;
        float cy     = kHoodH * 0.5f;
        Mat4 t = root * Mat4::translate({ frontX, cy, 0.f })
               * Mat4::scale({ kHoodL, kHoodH, kBodyW });
        objects.push_back({ meshCache.cube(), t, bodyMat });
    }

    // ---- Front bumper ----
    {
        float bx = kBodyL * 0.5f + 0.06f;
        Mat4 t = root * Mat4::translate({ bx, 0.2f, 0.f })
               * Mat4::scale({ 0.12f, 0.2f, kBodyW + 0.1f });
        objects.push_back({ meshCache.cube(), t, trimMat });
    }

    // ---- Rear bumper ----
    {
        float bx = -kBodyL * 0.5f - 0.06f;
        Mat4 t = root * Mat4::translate({ bx, 0.2f, 0.f })
               * Mat4::scale({ 0.12f, 0.18f, kBodyW + 0.08f });
        objects.push_back({ meshCache.cube(), t, trimMat });
    }

    // ---- Headlights (2 small bright cubes on front) ----
    {
        Material lightMat(Color3(0.9f, 0.85f, 0.6f), 8.0f, 0.5f);
        float hx = kBodyL * 0.5f + 0.01f;
        for (int side = -1; side <= 1; side += 2)
        {
            float hz = side * (kBodyW * 0.5f - 0.2f);
            Mat4 t = root * Mat4::translate({ hx, 0.45f, hz })
                   * Mat4::scale({ 0.04f, 0.1f, 0.18f });
            objects.push_back({ meshCache.cube(), t, lightMat });
        }
    }

    // ---- Taillights (2 small red cubes on rear) ----
    {
        Material tailMat(Color3(0.7f, 0.08f, 0.05f), 8.0f, 0.4f);
        float tx = -kBodyL * 0.5f - 0.01f;
        for (int side = -1; side <= 1; side += 2)
        {
            float tz = side * (kBodyW * 0.5f - 0.18f);
            Mat4 t = root * Mat4::translate({ tx, 0.45f, tz })
                   * Mat4::scale({ 0.04f, 0.08f, 0.16f });
            objects.push_back({ meshCache.cube(), t, tailMat });
        }
    }

    // ---- Side mirrors (small cubes sticking out) ----
    {
        float mx = -0.1f + kRoofL * 0.5f - 0.1f;  // near front of roof
        for (int side = -1; side <= 1; side += 2)
        {
            float mz = side * (kBodyW * 0.5f + 0.12f);
            // Mirror arm
            Mat4 arm = root * Mat4::translate({ mx, kBodyH - 0.05f, mz })
                     * Mat4::scale({ 0.06f, 0.04f, 0.15f });
            objects.push_back({ meshCache.cube(), arm, bodyMat });
            // Mirror face
            Mat4 face = root * Mat4::translate({ mx, kBodyH - 0.02f, mz + side * 0.04f })
                      * Mat4::scale({ 0.08f, 0.08f, 0.02f });
            objects.push_back({ meshCache.cube(), face, glassMat });
        }
    }

    // ---- Exhaust pipe (small cylinder under rear) ----
    {
        float ex = -kBodyL * 0.5f + 0.1f;
        Mat4 t = root * Mat4::translate({ ex, 0.08f, kBodyW * 0.3f })
               * Mat4::rotateZ(1.5707963f);  // lay along X
        objects.push_back({ meshCache.cylinder(0.04f, 0.2f, 6), t, rimMat });
    }

    // ---- Wheel well arches (dark trim above each wheel) ----
    {
        const float wx[2] = { kBodyL * 0.5f - 0.55f, -kBodyL * 0.5f + 0.55f };
        Material archMat = pal.metal;
        archMat.color.r *= 0.4f; archMat.color.g *= 0.4f; archMat.color.b *= 0.4f;
        for (int li = 0; li < 2; ++li)
        {
            for (int side = -1; side <= 1; side += 2)
            {
                float az = side * (kBodyW * 0.5f + 0.005f);
                Mat4 t = root * Mat4::translate({ wx[li], kWheelR * 1.6f, az })
                       * Mat4::scale({ kWheelR * 2.2f, 0.06f, 0.04f });
                objects.push_back({ meshCache.cube(), t, archMat });
            }
        }
    }

    // ---- Wheels ----
    const float wx[2] = { kBodyL * 0.5f - 0.55f, -kBodyL * 0.5f + 0.55f };
    const float wz[2] = { kBodyW * 0.5f + kWheelT * 0.5f,
                          -kBodyW * 0.5f - kWheelT * 0.5f };

    for (int li = 0; li < 2; ++li) {
        for (int si = 0; si < 2; ++si) {
            float cy = kWheelR;
            Mat4 t = root * Mat4::translate({ wx[li], cy, wz[si] })
                   * Mat4::rotateX(1.5707963f)
                   * Mat4::scale({ kWheelR / 0.3f, kWheelT / 1.8f, kWheelR / 0.3f });
            objects.push_back({ meshCache.cylinder(0.3f, 1.8f, 10), t, rimMat });
        }
    }

    // ---- Collider ----
    float halfExtent = (kBodyL > kBodyW ? kBodyL : kBodyW) * 0.5f;
    colliders.push_back(AABB(
        { x - halfExtent, groundY,         z - halfExtent },
        { x + halfExtent, groundY + kBodyH + kRoofH, z + halfExtent }
    ));
}
