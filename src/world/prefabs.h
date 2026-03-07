#pragma once
#include "core/vec_math.h"
#include "rendering/mesh.h"
#include "rendering/renderer.h"
#include "game/player.h"
#include "world/city_materials.h"
#include <vector>
#include <deque>

// A placed prefab: collection of scene objects, colliders, and lights
struct PrefabInstance {
    std::vector<SceneObject> objects;
    std::vector<AABB> colliders;
    std::vector<PointLight> lights;
    std::vector<SlopeCollider> slopes;
};

class Prefabs {
public:
    void init();  // Create shared mesh prototypes

    // Street furniture
    PrefabInstance streetlamp(const Vec3& pos, const Color3& lightColor);
    PrefabInstance trashCan(const Vec3& pos, const Material& mat);
    PrefabInstance dumpster(const Vec3& pos, const Material& mat);
    PrefabInstance fireBarrel(const Vec3& pos);
    PrefabInstance bench(const Vec3& pos, float yawAngle, const Material& mat);
    PrefabInstance crate(const Vec3& pos, float size, const Material& mat);
    PrefabInstance vehicleHusk(const Vec3& pos, float yawAngle, const Material& mat);
    PrefabInstance bollard(const Vec3& pos, const Material& mat);
    PrefabInstance barricade(const Vec3& pos, float yawAngle, const Material& mat);
    PrefabInstance rubblePile(const Vec3& pos, uint32_t seed, const Material& mat);

    // Building-mounted
    PrefabInstance antenna(const Vec3& pos, float height);
    PrefabInstance signBoard(const Vec3& pos, float width, float height,
                             float yawAngle, const Material& mat);
    PrefabInstance pipeRun(const Vec3& start, const Vec3& end, float radius,
                           const Material& mat);

    // Interior furniture
    PrefabInstance table(const Vec3& pos, const Material& mat);
    PrefabInstance shelfRack(const Vec3& pos, float yawAngle, const Material& mat);
    PrefabInstance terminal(const Vec3& pos, float yawAngle, const Material& mat);
    PrefabInstance pillar(const Vec3& pos, float height, float radius,
                          const Material& mat);
    PrefabInstance debris(const Vec3& pos, uint32_t seed, const Material& mat);

    // Structural elements (used by building generator)
    PrefabInstance staircase(const Vec3& basePos, float width, float height,
                             float depth, int rampAxis, bool rampPositive,
                             const Material& mat);
    PrefabInstance doorFrame(const Vec3& pos, float yawAngle, const Material& mat);

    // Access shared meshes (for building generator to reference)
    const Mesh& cubeMesh() const { return m_cube; }
    const Mesh& cylinderMesh() const { return m_cylinder; }
    const Mesh& sphereMesh() const { return m_sphere; }
    const Mesh& wedgeMesh() const { return m_wedge; }

private:
    Mesh m_cube;
    Mesh m_cylinder;
    Mesh m_thinCylinder;  // for poles, rails
    Mesh m_sphere;
    Mesh m_wedge;
    Mesh m_smallSphere;   // for lights, details
};
