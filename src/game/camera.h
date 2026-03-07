#pragma once
#include "core/vec_math.h"

class Camera {
public:
    Camera();

    // Set camera from external position/orientation (driven by Player)
    void setFromPlayer(const Vec3& eyePos, float yaw, float pitch);

    Mat4 viewMatrix() const;
    Mat4 projectionMatrix(int screenWidth, int screenHeight) const;

    Vec3 position() const { return m_pos; }
    Vec3 forward() const;
    Vec3 right() const;
    Vec3 up() const;

    float fov = 1.2f;  // ~69 degrees
    float nearPlane = 0.05f;
    float farPlane = 100.0f;

private:
    Vec3 m_pos;
    float m_yaw;    // Rotation around Y axis (radians)
    float m_pitch;  // Rotation around X axis (radians)
};
