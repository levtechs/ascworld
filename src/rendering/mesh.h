#pragma once
#include "core/vec_math.h"
#include "rendering/framebuffer.h" // for Color3
#include <vector>
#include <array>

struct Triangle {
    std::array<int, 3> indices;
};

struct Material {
    Color3 color;      // base diffuse color
    float shininess;   // specular exponent (0 = matte, higher = glossy)
    float specular;    // specular intensity [0,1]

    Material() : color(0.7f, 0.7f, 0.7f), shininess(16.0f), specular(0.3f) {}
    Material(const Color3& c, float shin = 16.0f, float spec = 0.3f)
        : color(c), shininess(shin), specular(spec) {}
};

struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<Triangle> faces;

    // Compute flat face normal for a triangle
    Vec3 faceNormal(int faceIdx) const;
};

// Object in the scene: mesh + transform + material
struct SceneObject {
    const Mesh* mesh;
    Mat4 transform;
    Material material;
};

// Shape factory functions
Mesh createCube();
Mesh createPlane(float width, float depth, int divisionsW = 1, int divisionsD = 1);
Mesh createSphere(int rings = 8, int sectors = 12, float radius = 1.0f);
Mesh createCylinder(float radius = 0.3f, float height = 1.8f, int sectors = 12);
// Wedge/ramp: unit box with top sloped from Y=0 at -Z to Y=1 at +Z
// Scale and rotate to get desired slope dimensions
Mesh createWedge();
