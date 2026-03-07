#pragma once
#include "network/net_common.h"
#include "rendering/mesh.h"
#include "core/vec_math.h"
#include <string>
#include <chrono>

class RemotePlayer {
public:
    RemotePlayer(PeerId id, const std::string& name);

    // Update with new network state (called when receiving PlayerNetState)
    void onNetworkState(const PlayerNetState& state);

    // Interpolate position for smooth rendering (call each frame)
    void update(float dt);

    // Get scene objects for rendering (body cylinder + head sphere)
    // These point to internal meshes, valid for the lifetime of this object
    const SceneObject& bodyObject() const { return m_bodyObj; }
    const SceneObject& headObject() const { return m_headObj; }

    PeerId peerId() const { return m_peerId; }
    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    Vec3 position() const { return m_displayPos; }
    float yaw() const { return m_displayYaw; }
    float pitch() const { return m_displayPitch; }

    // Check if player data is stale (no updates for a while)
    bool isStale(float timeoutSeconds = 5.0f) const;

    // Get the unique color for this player (based on peer ID)
    Color3 playerColor() const;

private:
    PeerId m_peerId;
    std::string m_name;

    // Network state (latest received)
    Vec3 m_targetPos;
    float m_targetYaw = 0;
    float m_targetPitch = 0;
    uint8_t m_flags = 0;

    // Interpolated display state
    Vec3 m_displayPos;
    float m_displayYaw = 0;
    float m_displayPitch = 0;

    // Timing
    std::chrono::steady_clock::time_point m_lastUpdate;

    // Rendering
    Mesh m_bodyMesh;
    Mesh m_headMesh;
    SceneObject m_bodyObj;
    SceneObject m_headObj;

    void updateTransform();
};
