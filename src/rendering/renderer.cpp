#include "rendering/renderer.h"
#include <algorithm>
#include <cmath>

Renderer::Renderer()
    : sunDir(Vec3(0.4f, 0.75f, 0.3f).normalized()),
      sunColor(1.0f, 0.95f, 0.85f),
      sunIntensity(0.85f),
      ambientColor(0.08f, 0.09f, 0.12f),
      fogColor(0.55f, 0.6f, 0.7f),   // match horizon haze
      fogStart(20.0f),
      fogEnd(55.0f),
      skyZenith(0.15f, 0.25f, 0.55f),
      skyHorizon(0.55f, 0.6f, 0.7f),
      groundFar(0.25f, 0.35f, 0.2f),
      sunAngularRadius(0.08f) {}

Color3 Renderer::shade(const Vec3& worldPos, const Vec3& normal, const Material& mat,
                        const Vec3& camPos, const std::vector<PointLight>& lights) {
    Color3 result = ambientColor * mat.color;

    Vec3 viewDir = (camPos - worldPos).normalized();

    // --- Directional light (sun) ---
    {
        float NdotL = std::max(0.0f, normal.dot(sunDir));
        Color3 diffuse = mat.color * sunColor * (NdotL * sunIntensity);
        result += diffuse;

        // Blinn-Phong specular
        if (mat.specular > 0.0f && NdotL > 0.0f) {
            Vec3 halfVec = (sunDir + viewDir).normalized();
            float NdotH = std::max(0.0f, normal.dot(halfVec));
            float spec = std::pow(NdotH, mat.shininess);
            result += sunColor * (spec * mat.specular * sunIntensity);
        }
    }

    // --- Point lights ---
    for (const auto& light : lights) {
        Vec3 toLight = light.position - worldPos;
        float dist = toLight.length();
        if (dist > light.radius || dist < 0.001f) continue;

        Vec3 L = toLight * (1.0f / dist);

        float atten = 1.0f / (1.0f + dist * dist * 0.3f);
        float fade = 1.0f - (dist / light.radius);
        fade = fade * fade;
        atten *= fade * light.intensity;

        float NdotL = std::max(0.0f, normal.dot(L));
        Color3 diffuse = mat.color * light.color * (NdotL * atten);
        result += diffuse;

        if (mat.specular > 0.0f && NdotL > 0.0f) {
            Vec3 halfVec = (L + viewDir).normalized();
            float NdotH = std::max(0.0f, normal.dot(halfVec));
            float spec = std::pow(NdotH, mat.shininess);
            result += light.color * (spec * mat.specular * atten);
        }
    }

    return result;
}

void Renderer::rasterizeTriangle(const ScreenVert& v0, const ScreenVert& v1, const ScreenVert& v2,
                                  const Color3& color, float fogFactor, Framebuffer& fb) {
    int w = fb.width();
    int h = fb.height();

    int minX = std::max(0, (int)std::floor(std::min({v0.x, v1.x, v2.x})));
    int maxX = std::min(w - 1, (int)std::ceil(std::max({v0.x, v1.x, v2.x})));
    int minY = std::max(0, (int)std::floor(std::min({v0.y, v1.y, v2.y})));
    int maxY = std::min(h - 1, (int)std::ceil(std::max({v0.y, v1.y, v2.y})));

    if (minX > maxX || minY > maxY) return;

    float dx01 = v1.x - v0.x, dy01 = v1.y - v0.y;
    float dx02 = v2.x - v0.x, dy02 = v2.y - v0.y;
    float denom = dx01 * dy02 - dx02 * dy01;
    if (std::abs(denom) < 1e-6f) return;
    float invDenom = 1.0f / denom;

    Color3 finalColor = lerp(color, fogColor, fogFactor);

    for (int py = minY; py <= maxY; py++) {
        for (int px = minX; px <= maxX; px++) {
            float cx = px + 0.5f - v0.x;
            float cy = py + 0.5f - v0.y;

            float u = (cx * dy02 - cy * dx02) * invDenom;
            float v = (cy * dx01 - cx * dy01) * invDenom;
            float w0 = 1.0f - u - v;

            if (u < 0 || v < 0 || w0 < 0) continue;

            float depth = w0 * v0.z + u * v1.z + v * v2.z;

            if (depth < fb.getDepth(px, py)) {
                fb.setPixel(px, py, depth, finalColor);
            }
        }
    }
}

void Renderer::renderSky(const Camera& camera, Framebuffer& fb) {
    int w = fb.width();
    int h = fb.height();

    Vec3 camFwd = camera.forward();
    Vec3 camRgt = camera.right();
    Vec3 camUp  = camera.up();

    float charAspect = 2.0f;
    float aspect = (float)w / ((float)h * charAspect);
    float fovScale = std::tan(camera.fov / 2.0f);

    for (int y = 0; y < h; y++) {
        float ndy = 1.0f - 2.0f * (y + 0.5f) / h;
        for (int x = 0; x < w; x++) {
            float ndx = 2.0f * (x + 0.5f) / w - 1.0f;
            Vec3 dir = (camFwd + camRgt * (ndx * aspect * fovScale) + camUp * (ndy * fovScale)).normalized();
            float vert = dir.y;

            Color3 skyColor;
            if (vert > 0.0f) {
                float t = std::min(1.0f, vert * 2.5f);
                t = t * t;
                skyColor = lerp(skyHorizon, skyZenith, t);
            } else {
                float t = std::min(1.0f, -vert * 4.0f);
                skyColor = lerp(skyHorizon, groundFar, t);
            }

            float sunAngle = std::acos(std::max(-1.0f, std::min(1.0f, dir.dot(sunDir))));
            if (sunAngle < sunAngularRadius) {
                float edge = sunAngle / sunAngularRadius;
                float intensity = 1.0f - edge * edge;
                Color3 sunGlow(1.0f, 0.95f, 0.7f);
                skyColor = lerp(skyColor, sunGlow, intensity * 0.9f + 0.1f);
            } else if (sunAngle < sunAngularRadius * 4.0f) {
                float t = 1.0f - (sunAngle - sunAngularRadius) / (sunAngularRadius * 3.0f);
                t = t * t * t;
                Color3 glowColor(0.9f, 0.75f, 0.4f);
                skyColor = lerp(skyColor, glowColor, t * 0.4f);
            }

            fb.setPixel(x, y, 1e30f, skyColor);
        }
    }
}

// --- Near-plane clipping ---

// A clip-space vertex for clipping purposes
struct ClipVertex {
    Vec4 clip;      // clip-space position
    Vec3 world;     // world-space position (for shading interpolation)
};

// Clip a polygon against the near plane (w >= nearW).
// Input: polygon vertices in clip space. Output: clipped polygon.
// Uses Sutherland-Hodgman algorithm.
static void clipNearPlane(const ClipVertex* in, int inCount,
                          ClipVertex* out, int& outCount, float nearW) {
    outCount = 0;
    if (inCount < 3) return;

    for (int i = 0; i < inCount; i++) {
        const ClipVertex& curr = in[i];
        const ClipVertex& next = in[(i + 1) % inCount];

        bool currInside = curr.clip.w >= nearW;
        bool nextInside = next.clip.w >= nearW;

        if (currInside) {
            out[outCount++] = curr;
        }

        if (currInside != nextInside) {
            // Compute intersection
            float t = (nearW - curr.clip.w) / (next.clip.w - curr.clip.w);
            ClipVertex interp;
            interp.clip.x = curr.clip.x + t * (next.clip.x - curr.clip.x);
            interp.clip.y = curr.clip.y + t * (next.clip.y - curr.clip.y);
            interp.clip.z = curr.clip.z + t * (next.clip.z - curr.clip.z);
            interp.clip.w = curr.clip.w + t * (next.clip.w - curr.clip.w);
            interp.world.x = curr.world.x + t * (next.world.x - curr.world.x);
            interp.world.y = curr.world.y + t * (next.world.y - curr.world.y);
            interp.world.z = curr.world.z + t * (next.world.z - curr.world.z);
            out[outCount++] = interp;
        }
    }
}

void Renderer::render(const std::vector<SceneObject>& objects,
                       const std::vector<PointLight>& lights,
                       const Camera& camera,
                       Framebuffer& fb) {
    renderSky(camera, fb);

    int screenW = fb.width();
    int screenH = fb.height();

    Mat4 view = camera.viewMatrix();
    Mat4 proj = camera.projectionMatrix(screenW, screenH);
    Mat4 viewProj = proj * view;
    Vec3 camPos = camera.position();
    float nearW = camera.nearPlane;

    for (const auto& obj : objects) {
        const Mesh* mesh = obj.mesh;
        const Mat4& model = obj.transform;
        const Material& mat = obj.material;

        Mat4 mvp = viewProj * model;

        for (int fi = 0; fi < (int)mesh->faces.size(); fi++) {
            const auto& tri = mesh->faces[fi];

            // Get world-space vertices
            Vec3 worldVerts[3];
            for (int i = 0; i < 3; i++) {
                Vec4 wp = model * Vec4(mesh->vertices[tri.indices[i]], 1.0f);
                worldVerts[i] = wp.xyz();
            }

            // Face normal in world space
            Vec3 edge1 = worldVerts[1] - worldVerts[0];
            Vec3 edge2 = worldVerts[2] - worldVerts[0];
            Vec3 normal = edge1.cross(edge2).normalized();

            // Check facing direction — flip normal if back-facing (to render both sides)
            Vec3 toCamera = (camPos - worldVerts[0]).normalized();
            float facing = normal.dot(toCamera);
            Vec3 shadingNormal = normal;
            if (facing < 0.0f) {
                shadingNormal = normal * -1.0f; // flip for correct lighting on back face
            }

            // Compute shading at triangle centroid (flat shading)
            Vec3 centroid = (worldVerts[0] + worldVerts[1] + worldVerts[2]) * (1.0f / 3.0f);
            Color3 shadedColor = shade(centroid, shadingNormal, mat, camPos, lights);

            // Fog
            float dist = (centroid - camPos).length();
            float fogFactor = (dist - fogStart) / (fogEnd - fogStart);
            fogFactor = std::max(0.0f, std::min(1.0f, fogFactor));

            // Transform to clip space
            ClipVertex clipVerts[3];
            for (int i = 0; i < 3; i++) {
                clipVerts[i].clip = mvp * Vec4(mesh->vertices[tri.indices[i]], 1.0f);
                clipVerts[i].world = worldVerts[i];
            }

            // Clip against near plane
            ClipVertex clipped[7]; // max 4 vertices after clipping a triangle against one plane
            int clippedCount = 0;
            clipNearPlane(clipVerts, 3, clipped, clippedCount, nearW);

            if (clippedCount < 3) continue;

            // Convert clipped vertices to screen space
            ScreenVert screenVerts[7];
            for (int i = 0; i < clippedCount; i++) {
                Vec3 ndc = clipped[i].clip.perspectiveDivide();
                screenVerts[i].x = (ndc.x + 1.0f) * 0.5f * screenW;
                screenVerts[i].y = (1.0f - ndc.y) * 0.5f * screenH;
                screenVerts[i].z = ndc.z;
                screenVerts[i].w = clipped[i].clip.w;
            }

            // Rasterize as a fan (works for convex polygons from clipping)
            for (int i = 1; i < clippedCount - 1; i++) {
                rasterizeTriangle(screenVerts[0], screenVerts[i], screenVerts[i + 1],
                                  shadedColor, fogFactor, fb);
            }
        }
    }
}
