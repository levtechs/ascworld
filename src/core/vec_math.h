#pragma once
#include <cmath>
#include <algorithm>

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }

    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const {
        float len = length();
        if (len < 1e-8f) return {0, 0, 0};
        return *this / len;
    }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(const Vec3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}

    Vec3 xyz() const { return {x, y, z}; }
    Vec3 perspectiveDivide() const {
        if (std::abs(w) < 1e-8f) return {x, y, z};
        return {x / w, y / w, z / w};
    }
};

struct Mat4 {
    float m[4][4] = {};

    static Mat4 identity() {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }

    Vec4 operator*(const Vec4& v) const {
        return {
            m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z + m[0][3]*v.w,
            m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z + m[1][3]*v.w,
            m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z + m[2][3]*v.w,
            m[3][0]*v.x + m[3][1]*v.y + m[3][2]*v.z + m[3][3]*v.w
        };
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                for (int k = 0; k < 4; k++)
                    r.m[i][j] += m[i][k] * o.m[k][j];
        return r;
    }

    static Mat4 translate(const Vec3& t) {
        Mat4 r = identity();
        r.m[0][3] = t.x; r.m[1][3] = t.y; r.m[2][3] = t.z;
        return r;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 r;
        r.m[0][0] = s.x; r.m[1][1] = s.y; r.m[2][2] = s.z; r.m[3][3] = 1.0f;
        return r;
    }

    static Mat4 rotateX(float angle) {
        Mat4 r = identity();
        float c = std::cos(angle), s = std::sin(angle);
        r.m[1][1] = c;  r.m[1][2] = -s;
        r.m[2][1] = s;  r.m[2][2] = c;
        return r;
    }

    static Mat4 rotateY(float angle) {
        Mat4 r = identity();
        float c = std::cos(angle), s = std::sin(angle);
        r.m[0][0] = c;  r.m[0][2] = s;
        r.m[2][0] = -s; r.m[2][2] = c;
        return r;
    }

    static Mat4 rotateZ(float angle) {
        Mat4 r = identity();
        float c = std::cos(angle), s = std::sin(angle);
        r.m[0][0] = c;  r.m[0][1] = -s;
        r.m[1][0] = s;  r.m[1][1] = c;
        return r;
    }

    // Perspective projection matrix
    // fov: vertical field of view in radians
    // aspect: width/height (already accounting for char aspect ratio)
    // near, far: clipping planes
    static Mat4 perspective(float fov, float aspect, float near, float far) {
        Mat4 r;
        float f = 1.0f / std::tan(fov / 2.0f);
        r.m[0][0] = f / aspect;
        r.m[1][1] = f;
        r.m[2][2] = (far + near) / (near - far);
        r.m[2][3] = (2.0f * far * near) / (near - far);
        r.m[3][2] = -1.0f;
        return r;
    }

    // Look-at view matrix
    static Mat4 lookAt(const Vec3& eye, const Vec3& forward, const Vec3& up) {
        Vec3 f = forward.normalized();
        Vec3 r = f.cross(up).normalized();
        Vec3 u = r.cross(f).normalized();

        Mat4 mat = identity();
        mat.m[0][0] = r.x;  mat.m[0][1] = r.y;  mat.m[0][2] = r.z;  mat.m[0][3] = -r.dot(eye);
        mat.m[1][0] = u.x;  mat.m[1][1] = u.y;  mat.m[1][2] = u.z;  mat.m[1][3] = -u.dot(eye);
        mat.m[2][0] = -f.x; mat.m[2][1] = -f.y; mat.m[2][2] = -f.z; mat.m[2][3] = f.dot(eye);
        return mat;
    }
};

inline float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
