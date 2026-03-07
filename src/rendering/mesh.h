#pragma once
#include "core/vec_math.h"
#include "rendering/framebuffer.h" // for Color3
#include <vector>
#include <array>
#include <map>
#include <deque>

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
    float boundingRadius = -1.0f; // cached bounding sphere radius (-1 = not computed)

    // Compute flat face normal for a triangle
    Vec3 faceNormal(int faceIdx) const;

    // Compute and cache the bounding sphere radius (max vertex distance from origin)
    float computeBoundingRadius();
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

// --------------------------------------------------------------------------
// MeshCache — shared mesh instances to avoid duplicate geometry
// --------------------------------------------------------------------------
// Instead of creating a new Mesh for every SceneObject, create each unique
// mesh shape once and share it via const pointers.  The cache owns the meshes
// (stored in a deque for pointer stability).

struct CylinderKey {
    float radius, height;
    int sectors;
    bool operator<(const CylinderKey& o) const {
        if (radius != o.radius) return radius < o.radius;
        if (height != o.height) return height < o.height;
        return sectors < o.sectors;
    }
};

struct SphereKey {
    int rings, sectors;
    float radius;
    bool operator<(const SphereKey& o) const {
        if (rings != o.rings) return rings < o.rings;
        if (sectors != o.sectors) return sectors < o.sectors;
        return radius < o.radius;
    }
};

class MeshCache {
public:
    MeshCache();

    const Mesh* cube()  const { return m_cube; }
    const Mesh* wedge() const { return m_wedge; }
    const Mesh* cylinder(float radius = 0.3f, float height = 1.8f, int sectors = 12);
    const Mesh* sphere(int rings = 8, int sectors = 12, float radius = 1.0f);

private:
    std::deque<Mesh> m_storage;       // owns all meshes
    Mesh* m_cube  = nullptr;
    Mesh* m_wedge = nullptr;
    std::map<CylinderKey, Mesh*> m_cylinders;
    std::map<SphereKey, Mesh*> m_spheres;
};
