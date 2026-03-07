#include "rendering/renderer.h"
#include <algorithm>
#include <cmath>

Renderer::Renderer()
    : sunDir(Vec3(0.3f, 0.35f, 0.5f).normalized()),
      sunColor(0.8f, 0.5f, 0.3f),
      sunIntensity(0.5f),
      ambientColor(0.12f, 0.10f, 0.08f),
      fogColor(0.08f, 0.06f, 0.05f),   // dark smog
      fogStart(30.0f),
      fogEnd(80.0f),
      skyZenith(0.05f, 0.03f, 0.08f),  // dark purple-gray
      skyHorizon(0.15f, 0.08f, 0.04f), // polluted orange-brown haze
      groundFar(0.06f, 0.05f, 0.04f),  // dark concrete
      sunAngularRadius(0.12f) {}

void Renderer::setCityAtmosphere(const Color3& fogCol, const Color3& ambientCol,
                                  float fStart, float fEnd) {
    fogColor = fogCol;
    ambientColor = ambientCol;
    fogStart = fStart;
    fogEnd = fEnd;
}

// --- Frustum plane extraction from VP matrix (Griess-Hartmann method) ---
void Renderer::extractFrustumPlanes(const Mat4& vp, FrustumPlane planes[6]) {
    // Left:   row3 + row0
    planes[0] = { vp.m[3][0]+vp.m[0][0], vp.m[3][1]+vp.m[0][1], vp.m[3][2]+vp.m[0][2], vp.m[3][3]+vp.m[0][3] };
    // Right:  row3 - row0
    planes[1] = { vp.m[3][0]-vp.m[0][0], vp.m[3][1]-vp.m[0][1], vp.m[3][2]-vp.m[0][2], vp.m[3][3]-vp.m[0][3] };
    // Bottom: row3 + row1
    planes[2] = { vp.m[3][0]+vp.m[1][0], vp.m[3][1]+vp.m[1][1], vp.m[3][2]+vp.m[1][2], vp.m[3][3]+vp.m[1][3] };
    // Top:    row3 - row1
    planes[3] = { vp.m[3][0]-vp.m[1][0], vp.m[3][1]-vp.m[1][1], vp.m[3][2]-vp.m[1][2], vp.m[3][3]-vp.m[1][3] };
    // Near:   row3 + row2
    planes[4] = { vp.m[3][0]+vp.m[2][0], vp.m[3][1]+vp.m[2][1], vp.m[3][2]+vp.m[2][2], vp.m[3][3]+vp.m[2][3] };
    // Far:    row3 - row2
    planes[5] = { vp.m[3][0]-vp.m[2][0], vp.m[3][1]-vp.m[2][1], vp.m[3][2]-vp.m[2][2], vp.m[3][3]-vp.m[2][3] };
    for (int i = 0; i < 6; i++) planes[i].normalize();
}

float Renderer::meshBoundingRadius(const Mesh& mesh) {
    // Use cached value if available (computed once by MeshCache or on first access)
    if (mesh.boundingRadius >= 0.0f) return mesh.boundingRadius;
    // Fallback: compute from vertices (should not happen with MeshCache)
    float maxDistSq = 0.0f;
    for (const auto& v : mesh.vertices) {
        float dsq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (dsq > maxDistSq) maxDistSq = dsq;
    }
    return std::sqrt(maxDistSq);
}

float Renderer::maxScaleFactor(const Mat4& m) {
    // Compute the length of each column vector (the 3 scale axes)
    float sx = std::sqrt(m.m[0][0]*m.m[0][0] + m.m[1][0]*m.m[1][0] + m.m[2][0]*m.m[2][0]);
    float sy = std::sqrt(m.m[0][1]*m.m[0][1] + m.m[1][1]*m.m[1][1] + m.m[2][1]*m.m[2][1]);
    float sz = std::sqrt(m.m[0][2]*m.m[0][2] + m.m[1][2]*m.m[1][2] + m.m[2][2]*m.m[2][2]);
    return std::max({sx, sy, sz});
}

// --- Spatial light grid ---
void Renderer::buildLightGrid(const std::vector<PointLight>& lights, float cellSize,
                               LightGrid& grid) {
    grid.clear();
    float invCell = 1.0f / cellSize;
    for (const auto& light : lights) {
        // Compute cells the light overlaps (its radius can span multiple cells)
        int minCX = (int)std::floor((light.position.x - light.radius) * invCell);
        int maxCX = (int)std::floor((light.position.x + light.radius) * invCell);
        int minCZ = (int)std::floor((light.position.z - light.radius) * invCell);
        int maxCZ = (int)std::floor((light.position.z + light.radius) * invCell);
        for (int cx = minCX; cx <= maxCX; cx++) {
            for (int cz = minCZ; cz <= maxCZ; cz++) {
                grid[{cx, cz}].push_back(&light);
            }
        }
    }
}

Color3 Renderer::shade(const Vec3& worldPos, const Vec3& normal, const Material& mat,
                        const Vec3& camPos,
                        const std::unordered_map<LightCellKey, std::vector<const PointLight*>, LightCellHash>& lightGrid,
                        float lightCellSize,
                        const PointLight* lightsBase) {
    Color3 result = ambientColor * mat.color;

    Vec3 viewDir = (camPos - worldPos).normalized();

    // --- Directional light (sun) ---
    {
        float NdotL = std::max(0.0f, normal.dot(sunDir));
        if (NdotL > 0.0f) {
            Color3 diffuse = mat.color * sunColor * (NdotL * sunIntensity);
            result += diffuse;

            // Blinn-Phong specular
            if (mat.specular > 0.0f) {
                Vec3 halfVec = (sunDir + viewDir).normalized();
                float NdotH = std::max(0.0f, normal.dot(halfVec));
                float spec = std::pow(NdotH, mat.shininess);
                result += sunColor * (spec * mat.specular * sunIntensity);
            }
        }
    }

    // --- Point lights via spatial grid (only nearby lights) ---
    float invCell = 1.0f / lightCellSize;
    int cx = (int)std::floor(worldPos.x * invCell);
    int cz = (int)std::floor(worldPos.z * invCell);

    ++m_shadeStamp;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            auto it = lightGrid.find({cx + dx, cz + dz});
            if (it == lightGrid.end()) continue;
            for (const PointLight* light : it->second) {
                size_t idx = (size_t)(light - lightsBase);
                if (m_lightVisited[idx] == m_shadeStamp) continue;
                m_lightVisited[idx] = m_shadeStamp;

                Vec3 toLight = light->position - worldPos;
                float dist = toLight.length();
                if (dist > light->radius || dist < 0.001f) continue;

                Vec3 L = toLight * (1.0f / dist);

                float NdotL = std::max(0.0f, normal.dot(L));
                if (NdotL <= 0.0f) continue;

                float atten = 1.0f / (1.0f + dist * dist * 0.3f);
                float fade = 1.0f - (dist / light->radius);
                fade = fade * fade;
                atten *= fade * light->intensity;

                Color3 diffuse = mat.color * light->color * (NdotL * atten);
                result += diffuse;

                if (mat.specular > 0.0f) {
                    Vec3 halfVec = (L + viewDir).normalized();
                    float NdotH = std::max(0.0f, normal.dot(halfVec));
                    float spec = std::pow(NdotH, mat.shininess);
                    result += light->color * (spec * mat.specular * atten);
                }
            }
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

    // City light pollution glow color (warm neon bleed near horizon)
    Color3 cityGlow(0.2f, 0.1f, 0.05f);

    // Precompute sun thresholds to avoid per-pixel acos
    float cosSunRadius = std::cos(sunAngularRadius);
    float cosSunHalo   = std::cos(sunAngularRadius * 4.0f);

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

                // City light pollution: warm glow band between horizon and vert 0.05
                if (vert < 0.05f) {
                    float glow = 1.0f - (vert / 0.05f);
                    glow = glow * glow;
                    skyColor = lerp(skyColor, cityGlow, glow * 0.3f);
                }
            } else {
                float t = std::min(1.0f, -vert * 4.0f);
                skyColor = lerp(skyHorizon, groundFar, t);
            }

            // Sun disc check using dot product (no acos needed for most pixels)
            float sunDot = dir.dot(sunDir);
            if (sunDot > cosSunRadius) {
                // Inside sun disc — only use acos for these few pixels
                float sunAngle = std::acos(std::max(-1.0f, std::min(1.0f, sunDot)));
                float edge = sunAngle / sunAngularRadius;
                float intensity = 1.0f - edge * edge;
                Color3 sunGlow(1.0f, 0.6f, 0.3f);
                skyColor = lerp(skyColor, sunGlow, intensity * 0.9f + 0.1f);
            } else if (sunDot > cosSunHalo) {
                // Halo region — use acos only for halo pixels
                float sunAngle = std::acos(std::max(-1.0f, std::min(1.0f, sunDot)));
                float t = 1.0f - (sunAngle - sunAngularRadius) / (sunAngularRadius * 3.0f);
                t = t * t * t;
                Color3 glowColor(0.6f, 0.35f, 0.15f);
                skyColor = lerp(skyColor, glowColor, t * 0.4f);
            }

            fb.setPixel(x, y, 1e30f, skyColor);
        }
    }
}

// --- Near-plane clipping ---

struct ClipVertex {
    Vec4 clip;
    Vec3 world;
};

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
                       const LightGrid& lightGrid,
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

    // --- Optimization 1: Extract frustum planes for culling ---
    FrustumPlane frustum[6];
    extractFrustumPlanes(viewProj, frustum);

    // Light grid is pre-built and passed in (static lights, built once during world gen)
    const PointLight* lightsBase = lights.empty() ? nullptr : lights.data();

    // Resize light visited stamp vector if needed (no per-shade-call allocation)
    if (m_lightVisited.size() < lights.size()) {
        m_lightVisited.resize(lights.size(), 0);
    }

    for (const auto& obj : objects) {
        const Mesh* mesh = obj.mesh;
        const Mat4& model = obj.transform;
        const Material& mat = obj.material;

        // --- Optimization 1+2: Frustum + distance culling per object ---
        // Compute world-space bounding sphere center (transform local origin)
        Vec4 centerW = model * Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        Vec3 center = centerW.xyz();

        // Compute bounding sphere radius (mesh radius * max scale)
        float localRadius = meshBoundingRadius(*mesh);
        float scale = maxScaleFactor(model);
        float worldRadius = localRadius * scale;

        // Distance culling: skip objects fully beyond fog end
        float distToCam = (center - camPos).length();
        if (distToCam - worldRadius > fogEnd) continue;

        // Frustum culling: test bounding sphere against all 6 planes
        bool culled = false;
        for (int i = 0; i < 6; i++) {
            if (frustum[i].distanceToPoint(center) < -worldRadius) {
                culled = true;
                break;
            }
        }
        if (culled) continue;

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

            // Back-face culling: skip triangles facing away from the camera
            Vec3 toCamera = (camPos - worldVerts[0]).normalized();
            float facing = normal.dot(toCamera);
            if (facing < 0.0f) continue;  // back-facing, skip
            Vec3 shadingNormal = normal;

            // Compute shading at triangle centroid (flat shading)
            Vec3 centroid = (worldVerts[0] + worldVerts[1] + worldVerts[2]) * (1.0f / 3.0f);

            // --- Optimization 2: Per-triangle fog early-exit ---
            float dist = (centroid - camPos).length();
            float fogFactor = (dist - fogStart) / (fogEnd - fogStart);
            fogFactor = std::max(0.0f, std::min(1.0f, fogFactor));
            if (fogFactor >= 0.999f) continue;  // fully fogged, skip

            // Shade using spatial light grid instead of all lights
            Color3 shadedColor = shade(centroid, shadingNormal, mat, camPos,
                                       lightGrid, LIGHT_CELL_SIZE, lightsBase);

            // Transform to clip space
            ClipVertex clipVerts[3];
            for (int i = 0; i < 3; i++) {
                clipVerts[i].clip = mvp * Vec4(mesh->vertices[tri.indices[i]], 1.0f);
                clipVerts[i].world = worldVerts[i];
            }

            // Clip against near plane
            ClipVertex clipped[7];
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
