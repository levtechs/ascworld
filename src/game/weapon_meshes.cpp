#include "game/weapon_meshes.h"
#include <cmath>

// -------------------------------------------------------------------------
// Construction — create all weapon meshes once
// -------------------------------------------------------------------------
WeaponMeshes::WeaponMeshes() {
    // ---- Saber ----
    // Handle: small dark cylinder
    m_saberHandle = createCylinder(0.035f, 0.18f, 6);
    m_saberHandleMat = Material(Color3(0.25f, 0.22f, 0.20f), 8.0f, 0.2f);

    // Blade: long thin bright cyan prism (use cube scaled thin+long)
    m_saberBlade = createCube();
    m_saberBladeMat = Material(Color3(0.3f, 0.9f, 1.0f), 64.0f, 0.8f); // bright cyan, very shiny

    // ---- Laser ----
    // Body: boxy pistol frame
    m_laserBody = createCube();
    m_laserBodyMat = Material(Color3(0.35f, 0.33f, 0.38f), 32.0f, 0.5f); // dark gunmetal

    // Barrel: thin cylinder
    m_laserBarrel = createCylinder(0.02f, 0.18f, 6);
    m_laserBarrelMat = Material(Color3(0.45f, 0.42f, 0.40f), 48.0f, 0.6f); // lighter metal

    // Grip: small angled cube
    m_laserGrip = createCube();
    m_laserGripMat = Material(Color3(0.30f, 0.22f, 0.15f), 8.0f, 0.2f); // brown grip

    // ---- Flashbang ----
    // Body: olive sphere
    m_flashbangBody = createSphere(5, 6, 0.07f);
    m_flashbangBodyMat = Material(Color3(0.35f, 0.38f, 0.25f), 16.0f, 0.3f); // olive green

    // Band: thin cylinder ring around middle
    m_flashbangBand = createCylinder(0.075f, 0.02f, 8);
    m_flashbangBandMat = Material(Color3(0.30f, 0.28f, 0.26f), 24.0f, 0.4f); // dark gray band

    // ---- Laser beam ----
    m_laserBeamMesh = createCube();
    m_laserBeamMat = Material(Color3(3.0f, 0.65f, 0.28f), 64.0f, 0.2f); // bright emissive-like beam

    // Compute bounding radii for all meshes
    m_saberHandle.computeBoundingRadius();
    m_saberBlade.computeBoundingRadius();
    m_laserBody.computeBoundingRadius();
    m_laserBarrel.computeBoundingRadius();
    m_laserGrip.computeBoundingRadius();
    m_flashbangBody.computeBoundingRadius();
    m_flashbangBand.computeBoundingRadius();
    m_laserBeamMesh.computeBoundingRadius();
}

// -------------------------------------------------------------------------
// Helper: base transform for held weapon (right side of screen)
// Uses camera right/up/forward vectors so the weapon stays fixed on screen
// regardless of how fast the player turns.
// -------------------------------------------------------------------------
Mat4 WeaponMeshes::heldBaseTransform(const Vec3& eyePos, float yaw, float pitch) const {
    // Compute camera basis vectors (same math as Camera::forward/right/up)
    float cy = std::cos(yaw), sy = std::sin(yaw);
    float cp = std::cos(pitch), sp = std::sin(pitch);

    Vec3 fwd(sy * cp, sp, -cy * cp);
    Vec3 right(cy, 0.0f, sy);
    Vec3 up = right.cross(fwd).normalized();

    // Position weapon at fixed offset in camera space:
    // slightly right, slightly down, forward in front of camera
    Vec3 weaponPos = eyePos + right * 0.22f + up * (-0.18f) + fwd * 0.45f;

    // Build a rotation matrix from camera basis so weapon faces the same direction
    // as the camera (the weapon's local -Z aligns with camera forward)
    Mat4 rot = Mat4::identity();
    rot.m[0][0] = right.x; rot.m[0][1] = up.x; rot.m[0][2] = -fwd.x;
    rot.m[1][0] = right.y; rot.m[1][1] = up.y; rot.m[1][2] = -fwd.y;
    rot.m[2][0] = right.z; rot.m[2][1] = up.z; rot.m[2][2] = -fwd.z;

    return Mat4::translate(weaponPos) * rot;
}

// -------------------------------------------------------------------------
// First-person held weapon objects
// -------------------------------------------------------------------------
std::vector<SceneObject> WeaponMeshes::getHeldObjects(
    ItemType type, const Vec3& eyePos,
    float yaw, float pitch, float attackProgress) const
{
    std::vector<SceneObject> objs;
    if (type == ItemType::None) return objs;

    Mat4 base = heldBaseTransform(eyePos, yaw, pitch);

    switch (type) {
    case ItemType::Saber: {
        // Idle points up, then fast downward slash, then recover upward
        float saberAngle = 0.72f; // idle up angle
        if (attackProgress > 0.0f) {
            if (attackProgress < 0.4f) {
                float t = attackProgress / 0.4f;
                saberAngle = 0.72f + (-1.05f - 0.72f) * t;
            } else {
                float t = (attackProgress - 0.4f) / 0.6f;
                saberAngle = -1.05f + (0.72f + 1.05f) * t;
            }
        }
        Mat4 swing = Mat4::rotateX(saberAngle);

        // Handle: at base position, pointing forward (along -Z)
        Mat4 handleT = base * swing
                      * Mat4::rotateX(-1.57f); // rotate cylinder to point forward
        objs.push_back(SceneObject{&m_saberHandle, handleT, m_saberHandleMat});

        // Blade: extends forward from handle tip
        // Cube is unit [-0.5,0.5], scale to thin long blade
        Mat4 bladeT = base * swing
                    * Mat4::translate(Vec3(0.0f, 0.0f, -0.35f)) // forward from handle
                    * Mat4::scale(Vec3(0.025f, 0.025f, 0.5f));   // thin and long
        objs.push_back(SceneObject{&m_saberBlade, bladeT, m_saberBladeMat});
        break;
    }

    case ItemType::Laser: {
        // Attack animation: slight recoil (kick back)
        float recoil = 0.0f;
        if (attackProgress > 0.0f) {
            if (attackProgress < 0.3f)
                recoil = attackProgress / 0.3f * 0.06f; // kick back
            else
                recoil = (1.0f - attackProgress) / 0.7f * 0.06f; // return
        }
        Mat4 recoilT = Mat4::translate(Vec3(0.0f, 0.0f, recoil));

        // Body: boxy frame
        Mat4 bodyT = base * recoilT
                   * Mat4::scale(Vec3(0.06f, 0.07f, 0.16f));
        objs.push_back(SceneObject{&m_laserBody, bodyT, m_laserBodyMat});

        // Barrel: extends forward from body
        Mat4 barrelT = base * recoilT
                     * Mat4::translate(Vec3(0.0f, 0.01f, -0.16f))
                     * Mat4::rotateX(-1.57f); // cylinder points forward
        objs.push_back(SceneObject{&m_laserBarrel, barrelT, m_laserBarrelMat});

        // Grip: below body, angled slightly
        Mat4 gripT = base * recoilT
                   * Mat4::translate(Vec3(0.0f, -0.07f, 0.02f))
                   * Mat4::rotateX(0.2f) // slight angle
                   * Mat4::scale(Vec3(0.04f, 0.08f, 0.04f));
        objs.push_back(SceneObject{&m_laserGrip, gripT, m_laserGripMat});
        break;
    }

    case ItemType::Flashbang: {
        // Attack animation: wind up and throw (hand moves forward)
        float throwOffset = 0.0f;
        if (attackProgress > 0.0f) {
            if (attackProgress < 0.4f)
                throwOffset = attackProgress / 0.4f * (-0.3f); // wind forward
            else
                throwOffset = 0.0f; // gone (thrown), no render after throw point
        }

        if (attackProgress < 0.4f || attackProgress == 0.0f) {
            Mat4 throwT = Mat4::translate(Vec3(0.0f, 0.0f, throwOffset));

            // Body sphere
            Mat4 bodyT = base * throwT;
            objs.push_back(SceneObject{&m_flashbangBody, bodyT, m_flashbangBodyMat});

            // Band around middle
            Mat4 bandT = base * throwT
                       * Mat4::translate(Vec3(0.0f, 0.0f, 0.0f));
            objs.push_back(SceneObject{&m_flashbangBand, bandT, m_flashbangBandMat});
        }
        break;
    }

    default:
        break;
    }

    return objs;
}

// -------------------------------------------------------------------------
// Third-person weapon objects (attached to remote player)
// Scaled ~2.5x larger than first-person so other players can see them.
// Supports attack animations when attackProgress is 0..1.
// -------------------------------------------------------------------------
std::vector<SceneObject> WeaponMeshes::getThirdPersonObjects(
    ItemType type, const Vec3& playerPos, float yaw, float pitch, float attackProgress) const
{
    std::vector<SceneObject> objs;
    if (type == ItemType::None) return objs;

    // Scale factor for third-person visibility
    constexpr float S = 2.5f;

    // Weapon offset: slightly right, hand height (~0.8m up), in front
    // Negate yaw because player yaw is camera convention (opposite of world rotation)
    // Apply pitch so weapon tilts with player's look direction
    Mat4 base = Mat4::translate(playerPos)
              * Mat4::rotateY(-yaw)
              * Mat4::translate(Vec3(0.2f, 0.8f, 0.0f))
              * Mat4::rotateX(pitch)
              * Mat4::translate(Vec3(0.0f, 0.0f, -0.4f));

    switch (type) {
    case ItemType::Saber: {
        float saberAngle = 0.72f;
        if (attackProgress > 0.0f) {
            if (attackProgress < 0.4f) {
                float t = attackProgress / 0.4f;
                saberAngle = 0.72f + (-1.05f - 0.72f) * t;
            } else {
                float t = (attackProgress - 0.4f) / 0.6f;
                saberAngle = -1.05f + (0.72f + 1.05f) * t;
            }
        }
        Mat4 swing = Mat4::rotateX(saberAngle);

        // Handle
        Mat4 handleT = base * swing
                      * Mat4::rotateX(-1.57f)
                      * Mat4::scale(Vec3(S, S, S));
        objs.push_back(SceneObject{&m_saberHandle, handleT, m_saberHandleMat});

        // Blade pointing forward
        Mat4 bladeT = base * swing
                    * Mat4::translate(Vec3(0.0f, 0.0f, -0.35f * S))
                    * Mat4::scale(Vec3(0.025f * S, 0.025f * S, 0.5f * S));
        objs.push_back(SceneObject{&m_saberBlade, bladeT, m_saberBladeMat});
        break;
    }

    case ItemType::Laser: {
        // Attack animation: slight recoil
        float recoil = 0.0f;
        if (attackProgress > 0.0f) {
            if (attackProgress < 0.3f)
                recoil = attackProgress / 0.3f * 0.06f * S;
            else
                recoil = (1.0f - attackProgress) / 0.7f * 0.06f * S;
        }
        Mat4 recoilT = Mat4::translate(Vec3(0.0f, 0.0f, recoil));

        Mat4 bodyT = base * recoilT
                   * Mat4::scale(Vec3(0.06f * S, 0.07f * S, 0.16f * S));
        objs.push_back(SceneObject{&m_laserBody, bodyT, m_laserBodyMat});

        Mat4 barrelT = base * recoilT
                     * Mat4::translate(Vec3(0.0f, 0.01f * S, -0.16f * S))
                     * Mat4::rotateX(-1.57f)
                     * Mat4::scale(Vec3(S, S, S));
        objs.push_back(SceneObject{&m_laserBarrel, barrelT, m_laserBarrelMat});

        Mat4 gripT = base * recoilT
                   * Mat4::translate(Vec3(0.0f, -0.07f * S, 0.02f * S))
                   * Mat4::rotateX(0.2f)
                   * Mat4::scale(Vec3(0.04f * S, 0.08f * S, 0.04f * S));
        objs.push_back(SceneObject{&m_laserGrip, gripT, m_laserGripMat});
        break;
    }

    case ItemType::Flashbang: {
        // Attack animation: wind up and throw
        float throwOffset = 0.0f;
        if (attackProgress > 0.0f) {
            if (attackProgress < 0.4f)
                throwOffset = attackProgress / 0.4f * (-0.3f * S);
        }

        if (attackProgress < 0.4f || attackProgress <= 0.0f) {
            Mat4 throwT = Mat4::translate(Vec3(0.0f, 0.0f, throwOffset));

            Mat4 bodyT = base * throwT * Mat4::scale(Vec3(S, S, S));
            objs.push_back(SceneObject{&m_flashbangBody, bodyT, m_flashbangBodyMat});

            Mat4 bandT = base * throwT * Mat4::scale(Vec3(S, S, S));
            objs.push_back(SceneObject{&m_flashbangBand, bandT, m_flashbangBandMat});
        }
        break;
    }

    default:
        break;
    }

    return objs;
}

// -------------------------------------------------------------------------
// Dropped weapon objects (on the ground)
// -------------------------------------------------------------------------
std::vector<SceneObject> WeaponMeshes::getDroppedObjects(
    ItemType type, const Vec3& worldPos, float time) const
{
    std::vector<SceneObject> objs;
    if (type == ItemType::None) return objs;

    // Small hover bob
    float bobY = std::sin(time * 2.0f) * 0.05f + 0.15f;
    // Slow spin
    float spin = time * 1.5f;

    Vec3 pos(worldPos.x, worldPos.y + bobY, worldPos.z);
    Mat4 base = Mat4::translate(pos) * Mat4::rotateY(spin);

    switch (type) {
    case ItemType::Saber: {
        // Saber lying flat, slightly tilted
        Mat4 tiltBase = base * Mat4::rotateZ(1.57f * 0.5f); // tilt 45 degrees

        Mat4 handleT = tiltBase * Mat4::rotateX(-1.57f);
        objs.push_back(SceneObject{&m_saberHandle, handleT, m_saberHandleMat});

        Mat4 bladeT = tiltBase
                    * Mat4::translate(Vec3(0.0f, 0.0f, -0.35f))
                    * Mat4::scale(Vec3(0.025f, 0.025f, 0.5f));
        objs.push_back(SceneObject{&m_saberBlade, bladeT, m_saberBladeMat});
        break;
    }

    case ItemType::Laser: {
        Mat4 bodyT = base * Mat4::scale(Vec3(0.06f, 0.07f, 0.16f));
        objs.push_back(SceneObject{&m_laserBody, bodyT, m_laserBodyMat});

        Mat4 barrelT = base
                     * Mat4::translate(Vec3(0.0f, 0.01f, -0.16f))
                     * Mat4::rotateX(-1.57f);
        objs.push_back(SceneObject{&m_laserBarrel, barrelT, m_laserBarrelMat});

        Mat4 gripT = base
                   * Mat4::translate(Vec3(0.0f, -0.07f, 0.02f))
                   * Mat4::rotateX(0.2f)
                   * Mat4::scale(Vec3(0.04f, 0.08f, 0.04f));
        objs.push_back(SceneObject{&m_laserGrip, gripT, m_laserGripMat});
        break;
    }

    case ItemType::Flashbang: {
        Mat4 bodyT = base;
        objs.push_back(SceneObject{&m_flashbangBody, bodyT, m_flashbangBodyMat});

        Mat4 bandT = base;
        objs.push_back(SceneObject{&m_flashbangBand, bandT, m_flashbangBandMat});
        break;
    }

    default:
        break;
    }

    return objs;
}

// -------------------------------------------------------------------------
// Laser beam visual — stretched thin cube from start to end
// -------------------------------------------------------------------------
std::vector<SceneObject> WeaponMeshes::getLaserBeamObjects(
    const Vec3& start, const Vec3& end) const
{
    std::vector<SceneObject> objs;

    Vec3 delta = end - start;
    float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (length < 0.01f) return objs;

    Vec3 dir = delta * (1.0f / length);
    Vec3 mid = start + delta * 0.5f;

    // Build a rotation matrix that aligns the local Z axis with `dir`
    // Pick an up vector that isn't parallel to dir
    Vec3 worldUp(0.0f, 1.0f, 0.0f);
    if (std::abs(dir.y) > 0.99f) worldUp = Vec3(1.0f, 0.0f, 0.0f);
    Vec3 right = worldUp.cross(dir).normalized();
    Vec3 up = dir.cross(right).normalized();

    Mat4 rot = Mat4::identity();
    rot.m[0][0] = right.x; rot.m[0][1] = up.x; rot.m[0][2] = dir.x;
    rot.m[1][0] = right.y; rot.m[1][1] = up.y; rot.m[1][2] = dir.y;
    rot.m[2][0] = right.z; rot.m[2][1] = up.z; rot.m[2][2] = dir.z;

    // Thick beam for ASCII visibility
    Mat4 transform = Mat4::translate(mid)
                   * rot
                   * Mat4::scale(Vec3(0.15f, 0.15f, length));

    objs.push_back(SceneObject{&m_laserBeamMesh, transform, m_laserBeamMat});
    return objs;
}
