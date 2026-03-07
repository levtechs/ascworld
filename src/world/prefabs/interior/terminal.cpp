#include "world/prefabs/interior/terminal.h"
#include "core/vec_math.h"

void placeTerminal(float x, float groundY, float z, float yawRad, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights, std::vector<AABB>& colliders)
{
    const float baseW = 0.6f;
    const float baseH = 1.1f;
    const float baseD = 0.5f;
    const float monW   = 0.5f;
    const float monT   = 0.05f;
    const float monH   = 0.4f;
    const float tiltRad = 15.0f * 3.14159265f / 180.0f;

    Mat4 base = Mat4::translate({x, groundY, z}) * Mat4::rotateY(yawRad);

    Material bodyMat = pal.metal;
    bodyMat.color.r *= 0.6f;
    bodyMat.color.g *= 0.6f;
    bodyMat.color.b *= 0.6f;

    // --- Terminal body ---
    {
        Mat4 local = Mat4::translate({0.0f, baseH * 0.5f, 0.0f})
                   * Mat4::scale({baseW, baseH, baseD});
        objects.push_back({meshCache.cube(), base * local, bodyMat});
    }

    // --- Side vent grilles (left and right) ---
    {
        Material ventMat = bodyMat;
        ventMat.color.r *= 0.5f; ventMat.color.g *= 0.5f; ventMat.color.b *= 0.5f;
        for (int side = -1; side <= 1; side += 2)
        {
            // 3 horizontal vent slats
            for (int v = 0; v < 3; ++v)
            {
                float vy = baseH * 0.4f + v * 0.12f;
                Mat4 local = Mat4::translate({side * (baseW * 0.5f + 0.005f), vy, 0.0f})
                           * Mat4::scale({0.01f, 0.04f, baseD * 0.5f});
                objects.push_back({meshCache.cube(), base * local, ventMat});
            }
        }
    }

    // --- Monitor face (glowing screen) ---
    {
        float monCenterY = baseH + monH * 0.5f;
        Mat4 local = Mat4::translate({0.0f, monCenterY, -(baseD * 0.5f - monT)})
                   * Mat4::rotateX(-tiltRad)
                   * Mat4::scale({monW, monH, monT});
        objects.push_back({meshCache.cube(), base * local, pal.accent});
    }

    // --- Monitor bezel ---
    {
        Material bezelMat = pal.accentDim;
        bezelMat.color.r *= 0.3f;
        bezelMat.color.g *= 0.3f;
        bezelMat.color.b *= 0.3f;
        float monCenterY = baseH + monH * 0.5f;
        Mat4 local = Mat4::translate({0.0f, monCenterY, -(baseD * 0.5f - monT * 2.2f)})
                   * Mat4::rotateX(-tiltRad)
                   * Mat4::scale({monW + 0.04f, monH + 0.04f, monT});
        objects.push_back({meshCache.cube(), base * local, bezelMat});
    }

    // --- Status LEDs on front face (3 small colored dots) ---
    {
        Material ledGreen(Color3(0.1f, 0.9f, 0.15f), 4.0f, 0.2f);
        Material ledRed(Color3(0.9f, 0.1f, 0.05f), 4.0f, 0.2f);
        Material ledAmber(Color3(0.9f, 0.6f, 0.05f), 4.0f, 0.2f);
        float fz = -(baseD * 0.5f + 0.005f);
        float ledY = baseH * 0.85f;
        float spacing = 0.06f;
        // Green
        Mat4 g = Mat4::translate({-spacing, ledY, fz}) * Mat4::scale({0.025f, 0.025f, 0.01f});
        objects.push_back({meshCache.cube(), base * g, ledGreen});
        // Amber
        Mat4 a = Mat4::translate({0.0f, ledY, fz}) * Mat4::scale({0.025f, 0.025f, 0.01f});
        objects.push_back({meshCache.cube(), base * a, ledAmber});
        // Red (off/dim)
        Mat4 r = Mat4::translate({spacing, ledY, fz}) * Mat4::scale({0.025f, 0.025f, 0.01f});
        objects.push_back({meshCache.cube(), base * r, ledRed});
    }

    // --- Keyboard tray ---
    {
        Material keyMat = pal.metal;
        keyMat.color.r *= 0.5f;
        keyMat.color.g *= 0.5f;
        keyMat.color.b *= 0.5f;
        Mat4 local = Mat4::translate({0.0f, baseH * 0.35f, baseD * 0.5f + 0.1f})
                   * Mat4::scale({0.45f, 0.03f, 0.2f});
        objects.push_back({meshCache.cube(), base * local, keyMat});
    }

    // --- Keyboard keys (small raised bumps on tray) ---
    {
        Material keyCapMat = bodyMat;
        keyCapMat.color.r *= 0.8f; keyCapMat.color.g *= 0.8f; keyCapMat.color.b *= 0.8f;
        for (int row = 0; row < 2; ++row)
        {
            float kz = baseD * 0.5f + 0.06f + row * 0.08f;
            Mat4 local = Mat4::translate({0.0f, baseH * 0.35f + 0.02f, kz})
                       * Mat4::scale({0.38f, 0.015f, 0.06f});
            objects.push_back({meshCache.cube(), base * local, keyCapMat});
        }
    }

    // --- Cable (thin cylinder running down the back) ---
    {
        Material cableMat(Color3(0.1f, 0.1f, 0.1f), 4.0f, 0.1f);
        float cableY = baseH * 0.3f;
        Mat4 local = Mat4::translate({0.1f, cableY, -(baseD * 0.5f + 0.03f)});
        objects.push_back({meshCache.cylinder(0.02f, baseH * 0.6f, 6), base * local, cableMat});
    }

    // --- Base feet (2 small pads) ---
    {
        for (int side = -1; side <= 1; side += 2)
        {
            Mat4 local = Mat4::translate({side * 0.2f, 0.015f, 0.0f})
                       * Mat4::scale({0.15f, 0.03f, baseD + 0.04f});
            objects.push_back({meshCache.cube(), base * local, bodyMat});
        }
    }

    // --- Glowing screen point light ---
    {
        float screenY = groundY + baseH + monH * 0.5f;
        PointLight pl;
        pl.position = {x, screenY, z};
        pl.color    = pal.accent.color;
        pl.intensity = 0.6f;
        pl.radius    = 2.0f;
        lights.push_back(pl);
    }

    // AABB
    colliders.push_back(AABB(
        {x - baseW * 0.5f, groundY,              z - baseD * 0.5f},
        {x + baseW * 0.5f, groundY + baseH + monH, z + baseD * 0.5f + 0.22f}
    ));
}
