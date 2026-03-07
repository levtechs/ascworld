#pragma once
#include <vector>
#include <string>

struct Color3 {
    float r, g, b;
    Color3() : r(0), g(0), b(0) {}
    Color3(float r, float g, float b) : r(r), g(g), b(b) {}

    Color3 operator+(const Color3& o) const { return {r + o.r, g + o.g, b + o.b}; }
    Color3 operator-(const Color3& o) const { return {r - o.r, g - o.g, b - o.b}; }
    Color3 operator*(float s) const { return {r * s, g * s, b * s}; }
    Color3 operator*(const Color3& o) const { return {r * o.r, g * o.g, b * o.b}; }
    Color3& operator+=(const Color3& o) { r += o.r; g += o.g; b += o.b; return *this; }

    Color3 clamped() const;
    float luminance() const { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }
};

inline Color3 lerp(const Color3& a, const Color3& b, float t) {
    return a + (b - a) * t;
}

class Framebuffer {
public:
    Framebuffer(int width, int height);

    void clear();
    void setPixel(int x, int y, float depth, const Color3& color);
    float getDepth(int x, int y) const;
    void render() const;

    // Apply a color tint to the entire framebuffer (multiply + bias towards tint color)
    void applyTint(const Color3& tint, float strength);

    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    char luminanceToChar(float lum) const;

    int m_width;
    int m_height;
    std::vector<Color3> m_colors;
    std::vector<float> m_depth;
};
