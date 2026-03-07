#include "world/prefabs/building_mounted/antenna.h"

static const float kMastRadius = 0.06f;
static const float kMastHeight = 3.0f;
static const float kArmW = 0.04f;
static const float kArmH = 0.04f;
static const float kArmL = 1.0f;
static const float kArmY = 2.5f;

void placeAntenna(
    float x, float rooftopY, float z,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects)
{
    Material metalMat = pal.metal;

    // ---- Mast base plate (square footing) ----
    {
        Mat4 t = Mat4::translate({ x, rooftopY + 0.03f, z })
               * Mat4::scale({ 0.35f, 0.06f, 0.35f });
        objects.push_back({ meshCache.cube(), t, metalMat });
    }

    // ---- Mast (vertical cylinder) ----
    // createCylinder goes from Y=0 to Y=height, so translate to rooftopY
    {
        Mat4 t = Mat4::translate({ x, rooftopY, z });
        objects.push_back({ meshCache.cylinder(kMastRadius, kMastHeight, 8), t, metalMat });
    }

    // ---- Cross-arm 1: along X axis ----
    {
        float cy = rooftopY + kArmY + kArmH * 0.5f;
        Mat4 t = Mat4::translate({ x, cy, z })
               * Mat4::scale({ kArmL, kArmH, kArmW });
        objects.push_back({ meshCache.cube(), t, metalMat });
    }

    // ---- Cross-arm 2: along Z axis ----
    {
        float cy = rooftopY + kArmY + kArmH * 1.5f;
        Mat4 t = Mat4::translate({ x, cy, z })
               * Mat4::scale({ kArmW, kArmH, kArmL });
        objects.push_back({ meshCache.cube(), t, metalMat });
    }

    // ---- Lower cross-arm pair (at 1/3 height for lattice look) ----
    {
        float cy = rooftopY + kMastHeight * 0.33f;
        Mat4 t1 = Mat4::translate({ x, cy, z })
                * Mat4::scale({ kArmL * 0.6f, kArmH, kArmW });
        objects.push_back({ meshCache.cube(), t1, metalMat });

        Mat4 t2 = Mat4::translate({ x, cy + kArmH, z })
                * Mat4::scale({ kArmW, kArmH, kArmL * 0.6f });
        objects.push_back({ meshCache.cube(), t2, metalMat });
    }

    // ---- Guy wires (4 thin diagonal cubes from top toward base) ----
    {
        float wireLen = 2.2f;
        float wireAngle = 1.1f;  // steep angle
        // 4 wires at 45-degree offsets
        for (int i = 0; i < 4; ++i)
        {
            float angle = i * 1.5707963f + 0.7854f;  // 45, 135, 225, 315 degrees
            float dx = 0.5f * (i < 2 ? 1.0f : -1.0f);
            float dz = 0.5f * ((i == 0 || i == 3) ? 1.0f : -1.0f);
            float midY = rooftopY + kMastHeight * 0.65f;
            Mat4 t = Mat4::translate({ x + dx * 0.4f, midY, z + dz * 0.4f })
                   * Mat4::rotateY(angle)
                   * Mat4::rotateZ(wireAngle)
                   * Mat4::scale({ 0.015f, wireLen, 0.015f });
            Material wireMat = metalMat;
            wireMat.color.r *= 0.7f; wireMat.color.g *= 0.7f; wireMat.color.b *= 0.7f;
            objects.push_back({ meshCache.cube(), t, wireMat });
        }
    }

    // ---- Small dish (flat cylinder near top, angled) ----
    {
        float dishY = rooftopY + kMastHeight * 0.75f;
        Mat4 t = Mat4::translate({ x + 0.15f, dishY, z })
               * Mat4::rotateZ(-0.5f)  // tilt dish outward
               * Mat4::rotateX(0.3f);
        objects.push_back({ meshCache.cylinder(0.15f, 0.04f, 8), t, metalMat });
    }

    // ---- Blinking light at very top (bright red sphere) ----
    {
        Material lightMat(Color3(0.9f, 0.05f, 0.02f), 4.0f, 0.2f);
        float topY = rooftopY + kMastHeight + 0.04f;
        Mat4 t = Mat4::translate({ x, topY, z });
        objects.push_back({ meshCache.sphere(4, 6, 0.04f), t, lightMat });
    }
}
