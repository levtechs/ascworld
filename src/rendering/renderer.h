#pragma once
#include "core/vec_math.h"
#include "rendering/mesh.h"
#include "rendering/framebuffer.h"
#include "game/camera.h"
#include <vector>

// Point light source
struct PointLight {
    Vec3 position;
    Color3 color;
    float intensity;    // brightness multiplier
    float radius;       // falloff radius (light reaches 0 at this distance)

    PointLight() : intensity(1.0f), radius(10.0f) {}
    PointLight(const Vec3& pos, const Color3& col, float intens = 1.0f, float rad = 10.0f)
        : position(pos), color(col), intensity(intens), radius(rad) {}
};

class Renderer {
public:
    Renderer();

    void render(const std::vector<SceneObject>& objects,
                const std::vector<PointLight>& lights,
                const Camera& camera,
                Framebuffer& fb);

    // Directional light (sun) - normalized direction toward the light
    Vec3 sunDir;
    Color3 sunColor;
    float sunIntensity;

    // Ambient light
    Color3 ambientColor;

    // Fog settings
    Color3 fogColor;
    float fogStart;     // distance where fog begins
    float fogEnd;       // distance where fog is fully opaque

private:
    struct ScreenVert {
        float x, y, z;  // screen x, y and clip-space depth
        float w;         // clip w (for perspective-correct interpolation)
    };

    // Transform a world-space vertex through the full pipeline to screen space
    bool projectVertex(const Vec3& worldPos, const Mat4& viewProj,
                       int screenW, int screenH, ScreenVert& out);

    // Compute Blinn-Phong shading for a surface point
    Color3 shade(const Vec3& worldPos, const Vec3& normal, const Material& mat,
                 const Vec3& camPos, const std::vector<PointLight>& lights);

    // Rasterize a single triangle with a solid color
    void rasterizeTriangle(const ScreenVert& v0, const ScreenVert& v1, const ScreenVert& v2,
                           const Color3& color, float fogFactor, Framebuffer& fb);

    // Fill framebuffer with sky gradient + sun before geometry
    void renderSky(const Camera& camera, Framebuffer& fb);

    // Sky colors
    Color3 skyZenith;   // color straight up
    Color3 skyHorizon;  // color at the horizon
    Color3 groundFar;   // color below horizon (distant ground haze)
    float sunAngularRadius; // in radians
};
