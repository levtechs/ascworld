#include "world/prefabs/interior/pillar.h"
#include "core/vec_math.h"

void placePillar(float x, float groundY, float z, float height, const DistrictPalette& pal,
    MeshCache& meshCache, std::vector<SceneObject>& objects, std::vector<AABB>& colliders)
{
    const float radius = 0.3f;

    // Main cylinder column
    // createCylinder goes from Y=0 to Y=height, so translate to groundY
    Mat4 transform = Mat4::translate({x, groundY, z});
    objects.push_back({meshCache.cylinder(radius, height), transform, pal.trim});

    // Decorative base cap — slightly wider flat disk
    {
        const float capH = 0.08f;
        const float capR = radius + 0.06f;
        Mat4 capBase = Mat4::translate({x, groundY, z});
        objects.push_back({meshCache.cylinder(capR, capH), capBase, pal.trim});
    }

    // Decorative top cap
    {
        const float capH = 0.08f;
        const float capR = radius + 0.06f;
        Mat4 capTop = Mat4::translate({x, groundY + height - capH, z});
        objects.push_back({meshCache.cylinder(capR, capH), capTop, pal.trim});
    }

    // AABB uses tight bounding box around cylinder
    colliders.push_back(AABB(
        {x - radius, groundY,          z - radius},
        {x + radius, groundY + height, z + radius}
    ));
}
