#pragma once
#include "rendering/mesh.h"
#include <cstdint>

// Character color choices (index 0-7)
static const Color3 CHARACTER_COLORS[] = {
    Color3(1.0f, 0.3f, 0.3f),   // 0: Red
    Color3(0.3f, 0.5f, 1.0f),   // 1: Blue
    Color3(0.3f, 0.9f, 0.4f),   // 2: Green
    Color3(1.0f, 0.9f, 0.3f),   // 3: Yellow
    Color3(0.8f, 0.3f, 0.9f),   // 4: Purple
    Color3(0.3f, 0.9f, 0.9f),   // 5: Cyan
    Color3(1.0f, 0.6f, 0.2f),   // 6: Orange
    Color3(1.0f, 0.5f, 0.7f),   // 7: Pink
};
static constexpr int NUM_CHARACTER_COLORS = 8;

static const char* CHARACTER_COLOR_NAMES[] = {
    "Red", "Blue", "Green", "Yellow", "Purple", "Cyan", "Orange", "Pink"
};

// Character design (body shape)
enum class CharacterDesign : uint8_t {
    Standard = 0,   // Cylinder body + sphere head (original)
    Blocky   = 1,   // Cube body + cube head (robot-like)
    Slim     = 2,   // Thin cylinder + small sphere head
    COUNT
};

static const char* CHARACTER_DESIGN_NAMES[] = {
    "Standard", "Blocky", "Slim"
};

struct CharacterAppearance {
    uint8_t colorIndex = 0;
    CharacterDesign design = CharacterDesign::Standard;

    Color3 color() const {
        return CHARACTER_COLORS[colorIndex % NUM_CHARACTER_COLORS];
    }

    // Pack into a single byte for network: bits 0-2 = color, bits 3-4 = design
    uint8_t serialize() const {
        return static_cast<uint8_t>((colorIndex & 0x07) | ((static_cast<uint8_t>(design) & 0x03) << 3));
    }

    static CharacterAppearance deserialize(uint8_t byte) {
        CharacterAppearance a;
        a.colorIndex = byte & 0x07;
        uint8_t d = (byte >> 3) & 0x03;
        if (d >= static_cast<uint8_t>(CharacterDesign::COUNT)) d = 0;
        a.design = static_cast<CharacterDesign>(d);
        return a;
    }
};
