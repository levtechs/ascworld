#include "barricade.h"

void placeBarricade(
    float x, float groundY, float z, float yawRad,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights,
    std::vector<AABB>& colliders
)
{
    // Dimensions: W=1.5, D=0.3, H=0.8
    const float W = 1.5f;
    const float D = 0.3f;
    const float H = 0.8f;

    // --- Main body ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + H * 0.5f, z))
               * Mat4::rotateY(yawRad)
               * Mat4::scale(Vec3(W, H, D));
        objects.push_back(SceneObject{ meshCache.cube(), t, pal.metal });
    }

    // --- Angled top-left trim wedge ---
    // Wedge slopes from Y=0 at -Z to Y=1 at +Z; we repurpose it here rotated 
    // to create a slanted cap on the left half of the top.
    {
        // Place at left half of barricade top, local space before rotation
        // Wedge unit is 1x1x1; scale to (W*0.5, 0.18, D)
        Mat4 t = Mat4::translate(Vec3(x, groundY + H, z))
               * Mat4::rotateY(yawRad)
               * Mat4::translate(Vec3(-W * 0.25f, 0.0f, 0.0f))
               * Mat4::scale(Vec3(W * 0.5f, 0.18f, D));
        objects.push_back(SceneObject{ meshCache.wedge(), t, pal.wallDark });
    }

    // --- Angled top-right trim wedge (mirrored via negative scale X) ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + H, z))
               * Mat4::rotateY(yawRad)
               * Mat4::translate(Vec3(W * 0.25f, 0.0f, 0.0f))
               * Mat4::scale(Vec3(-W * 0.5f, 0.18f, D));
        objects.push_back(SceneObject{ meshCache.wedge(), t, pal.wallDark });
    }

    // --- Warning stripe band (accent color) ---
    {
        Mat4 t = Mat4::translate(Vec3(x, groundY + H * 0.5f, z))
               * Mat4::rotateY(yawRad)
               * Mat4::scale(Vec3(W + 0.01f, 0.08f, D + 0.01f));
        objects.push_back(SceneObject{ meshCache.cube(), t, pal.accent });
    }

    // --- Collider ---
    float hw = W * 0.5f;
    float hd = D * 0.5f;
    colliders.push_back(AABB(
        Vec3(x - hw, groundY,        z - hd),
        Vec3(x + hw, groundY + H + 0.18f, z + hd)
    ));

    (void)lights;
}
