#pragma once
#include <cmath>
#include <cstdint>

// Simple Perlin-like noise implementation for procedural terrain generation
// Uses value noise with smooth interpolation (good enough for terrain)

class PerlinNoise {
public:
    explicit PerlinNoise(uint32_t seed = 0) : m_seed(seed) {}

    // 2D noise returning value in approximately [-1, 1]
    float noise2D(float x, float y) const {
        int xi = fastFloor(x);
        int yi = fastFloor(y);
        float xf = x - xi;
        float yf = y - yi;

        // Smooth interpolation
        float u = fade(xf);
        float v = fade(yf);

        // Hash corners
        float n00 = grad2D(hash2D(xi, yi), xf, yf);
        float n10 = grad2D(hash2D(xi + 1, yi), xf - 1.0f, yf);
        float n01 = grad2D(hash2D(xi, yi + 1), xf, yf - 1.0f);
        float n11 = grad2D(hash2D(xi + 1, yi + 1), xf - 1.0f, yf - 1.0f);

        // Bilinear interpolation
        float nx0 = lerp(n00, n10, u);
        float nx1 = lerp(n01, n11, u);
        return lerp(nx0, nx1, v);
    }

    // Fractal Brownian Motion - layered noise for natural terrain
    float fbm(float x, float y, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f) const {
        float sum = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maxAmplitude = 0.0f;

        for (int i = 0; i < octaves; i++) {
            sum += amplitude * noise2D(x * frequency, y * frequency);
            maxAmplitude += amplitude;
            amplitude *= gain;
            frequency *= lacunarity;
        }

        return sum / maxAmplitude; // Normalize to [-1, 1]
    }

private:
    uint32_t m_seed;

    static int fastFloor(float x) {
        int xi = (int)x;
        return x < xi ? xi - 1 : xi;
    }

    static float fade(float t) {
        // 6t^5 - 15t^4 + 10t^3 (improved Perlin fade)
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    static float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    // Hash function for 2D coordinates
    uint32_t hash2D(int x, int y) const {
        uint32_t h = m_seed;
        h ^= (uint32_t)x * 374761393u;
        h ^= (uint32_t)y * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h;
    }

    // Gradient function for 2D - picks a pseudo-random gradient direction
    static float grad2D(uint32_t hash, float x, float y) {
        uint32_t h = hash & 7u;
        float u = h < 4 ? x : y;
        float v = h < 4 ? y : x;
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
    }
};
