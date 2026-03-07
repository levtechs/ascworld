#include "rendering/framebuffer.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>
#include <algorithm>
#include <array>

// Precomputed gamma LUT: linear [0..4095] -> sRGB [0..255]
static const auto GAMMA_LUT = [] {
    std::array<uint8_t, 4096> lut{};
    for (int i = 0; i < 4096; i++) {
        float linear = static_cast<float>(i) / 4095.0f;
        float s = std::pow(linear, 1.0f / 2.2f);
        lut[i] = static_cast<uint8_t>(std::min(255, std::max(0, static_cast<int>(s * 255.0f + 0.5f))));
    }
    return lut;
}();

// Extended ASCII brightness ramp - 24 characters for finer gradation
// From empty/dark to dense/bright
static const char RAMP[] = " `.-':_,^=;><+!rc*/z?sLTv)J7(|Fi{C}fI31tlu[neoZ5Yxjya]2ESwqkP6h9d4VpOGbUAKXHm8RD#$W&B@MQ%N";
static const int RAMP_LEN = sizeof(RAMP) - 1;

Color3 Color3::clamped() const {
    return {std::max(0.0f, std::min(1.0f, r)),
            std::max(0.0f, std::min(1.0f, g)),
            std::max(0.0f, std::min(1.0f, b))};
}

Framebuffer::Framebuffer(int width, int height)
    : m_width(width), m_height(height),
      m_colors(width * height),
      m_depth(width * height, std::numeric_limits<float>::max()) {}

void Framebuffer::clear() {
    std::fill(m_colors.begin(), m_colors.end(), Color3(0, 0, 0));
    std::fill(m_depth.begin(), m_depth.end(), std::numeric_limits<float>::max());
}

void Framebuffer::setPixel(int x, int y, float depth, const Color3& color) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    int idx = y * m_width + x;
    if (depth < m_depth[idx]) {
        m_depth[idx] = depth;
        m_colors[idx] = color;
    }
}

float Framebuffer::getDepth(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return std::numeric_limits<float>::max();
    return m_depth[y * m_width + x];
}

void Framebuffer::render() const {
    // Build the entire frame as one big string with ANSI truecolor escapes
    // Format: \033[38;2;R;G;Bm<char>  (foreground color)
    // We also set background to a dimmed version for half-pixel blending feel

    std::string output;
    // Generous reserve: each pixel can be ~30 bytes for color escape
    output.reserve(m_width * m_height * 28 + m_height + 64);
    output += "\033[H"; // cursor home

    int prevR = -1, prevG = -1, prevB = -1;
    int prevBgR = -1, prevBgG = -1, prevBgB = -1;

    auto toSRGB = [](float linear) -> int {
        int idx = static_cast<int>(std::max(0.0f, std::min(1.0f, linear)) * 4095.0f + 0.5f);
        return GAMMA_LUT[idx];
    };

    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            const Color3& c = m_colors[y * m_width + x];
            Color3 cc = c.clamped();

            float lum = cc.luminance();
            char ch = luminanceToChar(lum);

            int ir = toSRGB(cc.r);
            int ig = toSRGB(cc.g);
            int ib = toSRGB(cc.b);

            // Background color: darker version of foreground
            int bgr = ir / 4;
            int bgg = ig / 4;
            int bgb = ib / 4;

            if (ch == ' ') {
                // Very dark or sky pixel — use a filled block with background color
                // Set both fg and bg to the pixel color for a solid look
                bgr = ir;
                bgg = ig;
                bgb = ib;
            }

            // Only emit escape codes when color changes
            bool fgChanged = (ir != prevR || ig != prevG || ib != prevB);
            bool bgChanged = (bgr != prevBgR || bgg != prevBgG || bgb != prevBgB);

            if (fgChanged && bgChanged) {
                char buf[48];
                int len = snprintf(buf, sizeof(buf),
                                   "\033[38;2;%d;%d;%d;48;2;%d;%d;%dm",
                                   ir, ig, ib, bgr, bgg, bgb);
                output.append(buf, len);
            } else if (fgChanged) {
                char buf[28];
                int len = snprintf(buf, sizeof(buf),
                                   "\033[38;2;%d;%d;%dm", ir, ig, ib);
                output.append(buf, len);
            } else if (bgChanged) {
                char buf[28];
                int len = snprintf(buf, sizeof(buf),
                                   "\033[48;2;%d;%d;%dm", bgr, bgg, bgb);
                output.append(buf, len);
            }

            prevR = ir; prevG = ig; prevB = ib;
            prevBgR = bgr; prevBgG = bgg; prevBgB = bgb;

            output += (ch == ' ') ? ' ' : ch;
        }
        // Reset at end of each line to avoid color bleeding
        if (prevR != -1) {
            output += "\033[0m";
            prevR = prevG = prevB = -1;
            prevBgR = prevBgG = prevBgB = -1;
        }
        if (y < m_height - 1) output += '\n';
    }

    // Final reset
    output += "\033[0m";

    fwrite(output.data(), 1, output.size(), stdout);
    fflush(stdout);
}

void Framebuffer::applyTint(const Color3& tint, float strength) {
    for (auto& c : m_colors) {
        c.r = c.r * (1.0f - strength) + c.r * tint.r * strength;
        c.g = c.g * (1.0f - strength) + c.g * tint.g * strength;
        c.b = c.b * (1.0f - strength) + c.b * tint.b * strength;
    }
}

char Framebuffer::luminanceToChar(float lum) const {
    if (lum <= 0.005f) return ' ';
    if (lum >= 1.0f) return RAMP[RAMP_LEN - 1];
    int idx = static_cast<int>(lum * (RAMP_LEN - 1));
    if (idx >= RAMP_LEN) idx = RAMP_LEN - 1;
    return RAMP[idx];
}
