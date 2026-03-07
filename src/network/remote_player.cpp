#include "network/remote_player.h"
#include <algorithm>
#include <cmath>

static const Color3 s_playerColors[] = {
    Color3(1.0f, 0.3f, 0.3f),  // 0: red
    Color3(0.3f, 0.5f, 1.0f),  // 1: blue
    Color3(0.3f, 0.9f, 0.4f),  // 2: green
    Color3(1.0f, 0.9f, 0.3f),  // 3: yellow
    Color3(0.8f, 0.3f, 0.9f),  // 4: purple
    Color3(0.3f, 0.9f, 0.9f),  // 5: cyan
    Color3(1.0f, 0.6f, 0.2f),  // 6: orange
    Color3(1.0f, 0.5f, 0.7f),  // 7: pink
};
static constexpr int s_numColors = 8;

RemotePlayer::RemotePlayer(PeerId id, const std::string& name)
    : m_peerId(id)
    , m_name(name)
    , m_lastUpdate(std::chrono::steady_clock::now())
{
    // Create meshes
    m_bodyMesh = createCylinder(0.3f, 1.4f, 8);
    m_headMesh = createSphere(6, 8, 0.25f);

    // Set up scene objects pointing to our internal meshes
    Color3 color = playerColor();
    Material mat(color, 16.0f, 0.3f);

    m_bodyObj.mesh = &m_bodyMesh;
    m_bodyObj.transform = Mat4::identity();
    m_bodyObj.material = mat;

    m_headObj.mesh = &m_headMesh;
    m_headObj.transform = Mat4::identity();
    m_headObj.material = mat;

    updateTransform();
}

Color3 RemotePlayer::playerColor() const {
    return s_playerColors[m_peerId % s_numColors];
}

void RemotePlayer::onNetworkState(const PlayerNetState& state) {
    m_targetPos = Vec3(state.x, state.y, state.z);
    m_targetYaw = state.yaw;
    m_targetPitch = state.pitch;
    m_flags = state.flags;
    m_lastUpdate = std::chrono::steady_clock::now();
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

    updateTransform();
}

void RemotePlayer::updateTransform() {
    Color3 color = playerColor();
    Material mat(color, 16.0f, 0.3f);

    // Body: translate to display position, rotate by yaw
    m_bodyObj.transform = Mat4::translate(Vec3(m_displayPos.x, m_displayPos.y, m_displayPos.z))
                        * Mat4::rotateY(m_displayYaw);
    m_bodyObj.material = mat;

    // Head: on top of body (body height is 1.4, so head at +1.5)
    m_headObj.transform = Mat4::translate(Vec3(m_displayPos.x, m_displayPos.y + 1.5f, m_displayPos.z));
    m_headObj.material = mat;
}

bool RemotePlayer::isStale(float timeoutSeconds) const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<float>(now - m_lastUpdate).count();
    return elapsed > timeoutSeconds;
}
