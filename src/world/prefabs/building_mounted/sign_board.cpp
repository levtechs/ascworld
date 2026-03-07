#include "world/prefabs/building_mounted/sign_board.h"
#include <cmath>

// Sign dimensions: W=1.8, D=0.08, H=0.6
// Mounted so its bottom sits at basePos.y, offset out from the wall by half D.
static const float kSignW = 1.8f;
static const float kSignD = 0.08f;
static const float kSignH = 0.6f;

// The sign is offset slightly away from the wall surface to float in front.
static const float kWallOffset = 0.06f;

void placeSignBoard(
    const Vec3& basePos, const Vec3& wallNormal,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights)
{
    // Sign face uses the accent (neon) colour.
    Material signMat = pal.accent;
    signMat.shininess = 32.f;
    signMat.specular  = 0.8f;  // glossy neon surface

    // Center of the sign box:
    //   - elevated by half sign height above basePos.y
    //   - pushed out from wall by (halfDepth + wallOffset)
    float halfD     = kSignD * 0.5f;
    float totalPush = halfD + kWallOffset;

    Vec3 center = {
        basePos.x + wallNormal.x * totalPush,
        basePos.y + kSignH * 0.5f,
        basePos.z + wallNormal.z * totalPush
    };

    // Compute yaw angle from wallNormal so the sign faces outward.
    // wallNormal is in XZ plane; atan2 gives us rotation around Y.
    float yawRad = std::atan2(wallNormal.x, wallNormal.z);

    // After rotation by yaw, local X becomes the sign width axis,
    // local Z becomes the depth axis (facing the wall normal).
    Mat4 transform = Mat4::translate(center)
                   * Mat4::rotateY(yawRad)
                   * Mat4::scale({ kSignW, kSignH, kSignD });

    objects.push_back({ meshCache.cube(), transform, signMat });

    // ---- Point light: neon glow emanating from sign face ----
    // Position the light slightly in front of the sign center.
    Vec3 lightPos = {
        center.x + wallNormal.x * (halfD + 0.1f),
        center.y,
        center.z + wallNormal.z * (halfD + 0.1f)
    };

    PointLight light;
    light.position  = lightPos;
    light.color     = pal.accent.color;
    light.intensity = 0.8f;
    light.radius    = 3.0f;
    lights.push_back(light);
}
