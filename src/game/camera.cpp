#include "game/camera.h"
#include <algorithm>
#include <cmath>

Camera::Camera()
    : m_pos(0.0f, 1.6f, 5.0f), m_yaw(0.0f), m_pitch(0.0f) {}

void Camera::setFromPlayer(const Vec3& eyePos, float yaw, float pitch) {
    m_pos = eyePos;
    m_yaw = yaw;
    m_pitch = pitch;
}

Vec3 Camera::forward() const {
    return Vec3(
        std::sin(m_yaw) * std::cos(m_pitch),
        std::sin(m_pitch),
        -std::cos(m_yaw) * std::cos(m_pitch)
    );
}

Vec3 Camera::right() const {
    return Vec3(std::cos(m_yaw), 0.0f, std::sin(m_yaw));
}

Vec3 Camera::up() const {
    return right().cross(forward()).normalized();
}

Mat4 Camera::viewMatrix() const {
    Vec3 fwd = forward();
    Vec3 worldUp(0.0f, 1.0f, 0.0f);
    return Mat4::lookAt(m_pos, fwd, worldUp);
}

Mat4 Camera::projectionMatrix(int screenWidth, int screenHeight) const {
    float charAspect = 2.0f;
    float aspect = (float)screenWidth / ((float)screenHeight * charAspect);
    return Mat4::perspective(fov, aspect, nearPlane, farPlane);
}
