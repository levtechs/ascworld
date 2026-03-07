#include "network/remote_player.h"
#include <algorithm>
#include <cmath>

RemotePlayer::RemotePlayer(PeerId id, const std::string& name)
    : m_peerId(id)
    , m_name(name)
    , m_lastUpdate(std::chrono::steady_clock::now())
{
    // Default appearance: color from peer ID, standard design
    m_appearance.colorIndex = id % NUM_CHARACTER_COLORS;
    m_appearance.design = CharacterDesign::Standard;

    rebuildMeshes();
    updateTransform();
}

void RemotePlayer::rebuildMeshes() {
    switch (m_appearance.design) {
        case CharacterDesign::Blocky:
            // Cube body (0.5 wide x 1.4 tall x 0.4 deep) + cube head (0.4 x 0.4 x 0.4)
            m_bodyMesh = createCube();
            m_headMesh = createCube();
            break;
        case CharacterDesign::Slim:
            // Thin cylinder (0.2r x 1.4h) + small sphere head (0.2r)
            m_bodyMesh = createCylinder(0.2f, 1.4f, 8);
            m_headMesh = createSphere(6, 8, 0.2f);
            break;
        case CharacterDesign::Standard:
        default:
            // Original: cylinder (0.3r x 1.4h) + sphere head (0.25r)
            m_bodyMesh = createCylinder(0.3f, 1.4f, 8);
            m_headMesh = createSphere(6, 8, 0.25f);
            break;
    }

    Color3 color = playerColor();
    Material mat(color, 16.0f, 0.3f);

    m_bodyObj.mesh = &m_bodyMesh;
    m_bodyObj.transform = Mat4::identity();
    m_bodyObj.material = mat;

    m_headObj.mesh = &m_headMesh;
    m_headObj.transform = Mat4::identity();
    m_headObj.material = mat;
}

Color3 RemotePlayer::playerColor() const {
    return m_appearance.color();
}

void RemotePlayer::setAppearance(const CharacterAppearance& appearance) {
    m_appearance = appearance;
    rebuildMeshes();
    updateTransform();
}

void RemotePlayer::onNetworkState(const PlayerNetState& state) {
    m_targetPos = Vec3(state.x, state.y, state.z);
    m_targetYaw = state.yaw;
    m_targetPitch = state.pitch;
    m_flags = state.flags;
    m_lastUpdate = std::chrono::steady_clock::now();

    // Decode weapon state from flags bits 2-5
    m_activeWeapon = decodeWeaponType(m_flags);
    m_isAttacking = decodeIsAttacking(m_flags);
    m_isDead = decodeIsDead(m_flags);
}

void RemotePlayer::update(float dt) {
    float t = std::min(1.0f, 10.0f * dt);

    // Linearly interpolate position
    m_displayPos.x += (m_targetPos.x - m_displayPos.x) * t;
    m_displayPos.y += (m_targetPos.y - m_displayPos.y) * t;
    m_displayPos.z += (m_targetPos.z - m_displayPos.z) * t;

    // Interpolate yaw and pitch
    m_displayYaw += (m_targetYaw - m_displayYaw) * t;
    m_displayPitch += (m_targetPitch - m_displayPitch) * t;

    // Death animation: smoothly fall over
    float deathTarget = m_isDead ? 1.0f : 0.0f;
    float deathSpeed = m_isDead ? 3.0f : 6.0f; // fall slower, stand up faster
    m_deathAnimProgress += (deathTarget - m_deathAnimProgress) * std::min(1.0f, deathSpeed * dt);

    updateTransform();
}

void RemotePlayer::updateTransform() {
    Color3 color = playerColor();
    Material mat(color, 16.0f, 0.3f);

    // When dead, darken the color
    if (m_deathAnimProgress > 0.01f) {
        float darkFactor = 1.0f - m_deathAnimProgress * 0.5f;
        mat = Material(Color3(color.r * darkFactor, color.g * darkFactor, color.b * darkFactor),
                       16.0f, 0.1f);
    }

    float fallAngle = m_deathAnimProgress * 1.57f; // 0 to ~PI/2 (90 degrees)

    if (m_appearance.design == CharacterDesign::Blocky) {
        // Blocky: scale cube for body (0.5 x 1.4 x 0.4) and head (0.4 x 0.4 x 0.4)
        if (m_deathAnimProgress > 0.01f) {
            // Fallen body: rotate around feet (X axis) so body falls forward
            m_bodyObj.transform = Mat4::translate(Vec3(m_displayPos.x, m_displayPos.y, m_displayPos.z))
                                * Mat4::rotateY(m_displayYaw)
                                * Mat4::rotateX(-fallAngle)
                                * Mat4::scale(Vec3(0.5f, 1.4f, 0.4f));
            m_headObj.transform = Mat4::translate(Vec3(m_displayPos.x, m_displayPos.y, m_displayPos.z))
                                * Mat4::rotateY(m_displayYaw)
                                * Mat4::rotateX(-fallAngle)
                                * Mat4::translate(Vec3(0.0f, 1.5f, 0.0f))
                                * Mat4::scale(Vec3(0.4f, 0.4f, 0.4f));
        } else {
            m_bodyObj.transform = Mat4::translate(Vec3(m_displayPos.x, m_displayPos.y, m_displayPos.z))
                                * Mat4::rotateY(m_displayYaw)
                                * Mat4::scale(Vec3(0.5f, 1.4f, 0.4f));
            m_headObj.transform = Mat4::translate(Vec3(m_displayPos.x, m_displayPos.y + 1.5f, m_displayPos.z))
                                * Mat4::scale(Vec3(0.4f, 0.4f, 0.4f));
        }
    } else {
        // Standard and Slim designs
        if (m_deathAnimProgress > 0.01f) {
            // Fallen body: rotate around feet so body falls forward
            m_bodyObj.transform = Mat4::translate(Vec3(m_displayPos.x, m_displayPos.y, m_displayPos.z))
                                * Mat4::rotateY(m_displayYaw)
                                * Mat4::rotateX(-fallAngle);
            m_headObj.transform = Mat4::translate(Vec3(m_displayPos.x, m_displayPos.y, m_displayPos.z))
                                * Mat4::rotateY(m_displayYaw)
                                * Mat4::rotateX(-fallAngle)
                                * Mat4::translate(Vec3(0.0f, 1.5f, 0.0f));
        } else {
            // Body: translate to display position, rotate by yaw
            m_bodyObj.transform = Mat4::translate(Vec3(m_displayPos.x, m_displayPos.y, m_displayPos.z))
                                * Mat4::rotateY(m_displayYaw);
            // Head: on top of body
            m_headObj.transform = Mat4::translate(Vec3(m_displayPos.x, m_displayPos.y + 1.5f, m_displayPos.z));
        }
    }

    m_bodyObj.material = mat;
    m_headObj.material = mat;
}

bool RemotePlayer::isStale(float timeoutSeconds) const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<float>(now - m_lastUpdate).count();
    return elapsed > timeoutSeconds;
}
