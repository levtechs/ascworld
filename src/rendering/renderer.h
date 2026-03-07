#pragma once
#include "core/vec_math.h"
#include "rendering/mesh.h"
#include "rendering/framebuffer.h"
#include "game/camera.h"
#include <vector>
#include <unordered_map>

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

// Frustum plane (normal + distance from origin)
struct FrustumPlane {
    float a, b, c, d;  // ax + by + cz + d = 0
    void normalize() {
        float len = std::sqrt(a*a + b*b + c*c);
        if (len > 1e-8f) { a /= len; b /= len; c /= len; d /= len; }
    }
    float distanceToPoint(const Vec3& p) const {
        return a * p.x + b * p.y + c * p.z + d;
    }
};

// Spatial hash cell key for light grid
struct LightCellKey {
    int x, z;
    bool operator==(const LightCellKey& o) const { return x == o.x && z == o.z; }
};

struct LightCellHash {
    size_t operator()(const LightCellKey& k) const {
        return std::hash<int>()(k.x) ^ (std::hash<int>()(k.z) << 16);
    }
};

// Type alias for the spatial light grid
using LightGrid = std::unordered_map<LightCellKey, std::vector<const PointLight*>, LightCellHash>;

static constexpr float LIGHT_CELL_SIZE = 12.0f;

class Renderer {
public:
    Renderer();

    void render(const std::vector<SceneObject>& objects,
                const std::vector<PointLight>& lights,
                const LightGrid& lightGrid,
                const Camera& camera,
                Framebuffer& fb);

    // Override atmosphere per district (fog color, ambient, fog range)
    void setCityAtmosphere(const Color3& fogCol, const Color3& ambientCol, float fStart, float fEnd);

    // Build spatial hash grid for point lights (public for World to pre-build)
    static void buildLightGrid(const std::vector<PointLight>& lights, float cellSize,
                                LightGrid& grid);

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

    // Compute Blinn-Phong shading for a surface point using spatial light grid
    Color3 shade(const Vec3& worldPos, const Vec3& normal, const Material& mat,
                 const Vec3& camPos,
                 const LightGrid& lightGrid,
                 float lightCellSize,
                 const PointLight* lightsBase);

    // Rasterize a single triangle with a solid color
    void rasterizeTriangle(const ScreenVert& v0, const ScreenVert& v1, const ScreenVert& v2,
                           const Color3& color, float fogFactor, Framebuffer& fb);

    // Fill framebuffer with sky gradient + sun before geometry
    void renderSky(const Camera& camera, Framebuffer& fb);

    // Extract 6 frustum planes from a view-projection matrix
    static void extractFrustumPlanes(const Mat4& vp, FrustumPlane planes[6]);

    // Compute bounding sphere radius for a mesh (max vertex distance from origin)
    static float meshBoundingRadius(const Mesh& mesh);

    // Get max scale factor from a transform matrix
    static float maxScaleFactor(const Mat4& m);

    // Sky colors
    Color3 skyZenith;   // color straight up
    Color3 skyHorizon;  // color at the horizon
    Color3 groundFar;   // color below horizon (distant ground haze)
    float sunAngularRadius; // in radians

    // Per-shade-call light dedup stamp (avoids heap alloc per triangle)
    uint32_t m_shadeStamp = 0;
    std::vector<uint32_t> m_lightVisited;
};
