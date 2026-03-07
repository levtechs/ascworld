#include "rendering/mesh.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Vec3 Mesh::faceNormal(int faceIdx) const {
    const auto& tri = faces[faceIdx];
    Vec3 a = vertices[tri.indices[0]];
    Vec3 b = vertices[tri.indices[1]];
    Vec3 c = vertices[tri.indices[2]];
    Vec3 edge1 = b - a;
    Vec3 edge2 = c - a;
    return edge1.cross(edge2).normalized();
}

Mesh createCube() {
    Mesh m;
    // 8 vertices of a unit cube centered at origin
    m.vertices = {
        {-0.5f, -0.5f, -0.5f}, {+0.5f, -0.5f, -0.5f},
        {+0.5f, +0.5f, -0.5f}, {-0.5f, +0.5f, -0.5f},
        {-0.5f, -0.5f, +0.5f}, {+0.5f, -0.5f, +0.5f},
        {+0.5f, +0.5f, +0.5f}, {-0.5f, +0.5f, +0.5f},
    };
    // 12 triangles (2 per face)
    m.faces = {
        // Front (-Z)
        {{{0, 1, 2}}}, {{{0, 2, 3}}},
        // Back (+Z)
        {{{5, 4, 7}}}, {{{5, 7, 6}}},
        // Left (-X)
        {{{4, 0, 3}}}, {{{4, 3, 7}}},
        // Right (+X)
        {{{1, 5, 6}}}, {{{1, 6, 2}}},
        // Top (+Y)
        {{{3, 2, 6}}}, {{{3, 6, 7}}},
        // Bottom (-Y)
        {{{4, 5, 1}}}, {{{4, 1, 0}}},
    };
    return m;
}

Mesh createPlane(float width, float depth, int divisionsW, int divisionsD) {
    Mesh m;
    float halfW = width / 2.0f;
    float halfD = depth / 2.0f;

    for (int dz = 0; dz <= divisionsD; dz++) {
        for (int dx = 0; dx <= divisionsW; dx++) {
            float x = -halfW + (width * dx) / divisionsW;
            float z = -halfD + (depth * dz) / divisionsD;
            m.vertices.push_back({x, 0.0f, z});
        }
    }

    int cols = divisionsW + 1;
    for (int dz = 0; dz < divisionsD; dz++) {
        for (int dx = 0; dx < divisionsW; dx++) {
            int tl = dz * cols + dx;
            int tr = tl + 1;
            int bl = (dz + 1) * cols + dx;
            int br = bl + 1;
            m.faces.push_back({{{tl, bl, tr}}});
            m.faces.push_back({{{tr, bl, br}}});
        }
    }
    return m;
}

Mesh createCylinder(float radius, float height, int sectors) {
    Mesh m;
    // Bottom center = vertex 0, top center = vertex 1
    m.vertices.push_back({0.0f, 0.0f, 0.0f});        // 0: bottom center
    m.vertices.push_back({0.0f, height, 0.0f});        // 1: top center

    // Ring vertices: bottom ring starts at index 2, top ring at 2+sectors
    for (int i = 0; i < sectors; i++) {
        float theta = 2.0f * M_PI * i / sectors;
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        m.vertices.push_back({x, 0.0f, z});            // bottom ring
    }
    for (int i = 0; i < sectors; i++) {
        float theta = 2.0f * M_PI * i / sectors;
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        m.vertices.push_back({x, height, z});           // top ring
    }

    int botStart = 2;
    int topStart = 2 + sectors;

    for (int i = 0; i < sectors; i++) {
        int next = (i + 1) % sectors;

        // Bottom cap (normal pointing down) — winding: center, next, cur
        m.faces.push_back({{{0, botStart + next, botStart + i}}});

        // Top cap (normal pointing up) — winding: center, cur, next
        m.faces.push_back({{{1, topStart + i, topStart + next}}});

        // Side quad as two triangles (normals pointing outward)
        m.faces.push_back({{{botStart + i, botStart + next, topStart + i}}});
        m.faces.push_back({{{topStart + i, botStart + next, topStart + next}}});
    }

    return m;
}

Mesh createWedge() {
    Mesh m;
    // A wedge/ramp: base is a unit box [-0.5, 0.5] in X and Z.
    // Bottom face at Y=0. Top slopes from Y=0 at -Z to Y=1 at +Z.
    //
    // Vertices:
    //   Bottom: 0=(-0.5, 0, -0.5), 1=(+0.5, 0, -0.5), 2=(+0.5, 0, +0.5), 3=(-0.5, 0, +0.5)
    //   Top:    4=(-0.5, 0, -0.5), 5=(+0.5, 0, -0.5), 6=(+0.5, 1, +0.5), 7=(-0.5, 1, +0.5)
    // Note: top vertices at -Z are at Y=0 (same as bottom), at +Z are at Y=1
    m.vertices = {
        {-0.5f, 0.0f, -0.5f},  // 0: bottom-left-back
        {+0.5f, 0.0f, -0.5f},  // 1: bottom-right-back
        {+0.5f, 0.0f, +0.5f},  // 2: bottom-right-front
        {-0.5f, 0.0f, +0.5f},  // 3: bottom-left-front
        {-0.5f, 1.0f, +0.5f},  // 4: top-left-front
        {+0.5f, 1.0f, +0.5f},  // 5: top-right-front
    };
    // Bottom face (Y=0)
    m.faces.push_back({{{0, 2, 1}}});
    m.faces.push_back({{{0, 3, 2}}});
    // Slope face (from back-bottom to front-top)
    m.faces.push_back({{{0, 1, 5}}});
    m.faces.push_back({{{0, 5, 4}}});
    // Front face (+Z, vertical triangle: 3, 2, 5, 4)
    m.faces.push_back({{{3, 5, 4}}});  // note: only the top part is a triangle
    m.faces.push_back({{{3, 2, 5}}});
    // Left face (-X: 0, 3, 4)
    m.faces.push_back({{{0, 4, 3}}});
    // Right face (+X: 1, 2, 5)
    m.faces.push_back({{{1, 2, 5}}});
    return m;
}

Mesh createSphere(int rings, int sectors, float radius) {
    Mesh m;
    for (int r = 0; r <= rings; r++) {
        float phi = M_PI * r / rings;
        for (int s = 0; s <= sectors; s++) {
            float theta = 2.0f * M_PI * s / sectors;
            float x = radius * std::sin(phi) * std::cos(theta);
            float y = radius * std::cos(phi);
            float z = radius * std::sin(phi) * std::sin(theta);
            m.vertices.push_back({x, y, z});
        }
    }

    int cols = sectors + 1;
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < sectors; s++) {
            int cur = r * cols + s;
            int next = cur + cols;
            m.faces.push_back({{{cur, next, cur + 1}}});
            m.faces.push_back({{{cur + 1, next, next + 1}}});
        }
    }
    return m;
}
