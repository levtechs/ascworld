#pragma once
#include "rendering/framebuffer.h"
#include "rendering/mesh.h"

enum class District {
    Downtown,     // Dense skyscrapers, neon signs
    Tech,         // Labs, server buildings, clean
    Industrial,   // Factories, warehouses, pipes
    Slums,        // Shanties, stacked shacks
    Residential,  // Houses, apartments, faded
    Wasteland     // Rubble, foundations, bleached
};

// Material palette for each district — provides colors for walls, floors, trim,
// accent, emissive elements
struct DistrictPalette {
    Material wallPrimary;     // Main wall material
    Material wallSecondary;   // Alternate wall (variety)
    Material floor;           // Ground/floor surface
    Material trim;            // Window frames, edges
    Material accent;          // Signs, highlights
    Material roof;            // Rooftop surface
    Material metal;           // Pipes, railings, structural
    Color3 emissiveColor;     // Neon/glow color for this district
    float emissiveIntensity;  // How bright emissive elements are (0-1)

    // Lighting parameters for this district
    Color3 ambientColor;
    Color3 fogColor;
    float fogStart;
    float fogEnd;
};

// Returns the palette for a given district
inline DistrictPalette getDistrictPalette(District d) {
    switch (d) {

    case District::Downtown: {
        DistrictPalette p;
        p.wallPrimary   = Material(Color3(0.30f, 0.30f, 0.35f), 24.0f, 0.25f);
        p.wallSecondary = Material(Color3(0.22f, 0.22f, 0.28f), 20.0f, 0.20f);
        p.floor         = Material(Color3(0.18f, 0.18f, 0.20f), 10.0f, 0.15f);
        p.trim          = Material(Color3(0.40f, 0.42f, 0.48f), 48.0f, 0.50f);
        p.accent        = Material(Color3(0.10f, 0.60f, 0.80f), 64.0f, 0.60f);
        p.roof          = Material(Color3(0.20f, 0.20f, 0.22f), 12.0f, 0.10f);
        p.metal         = Material(Color3(0.15f, 0.15f, 0.20f), 80.0f, 0.70f);
        p.emissiveColor     = Color3(0.0f, 0.9f, 1.0f);
        p.emissiveIntensity = 0.85f;
        p.ambientColor = Color3(0.02f, 0.025f, 0.04f);
        p.fogColor     = Color3(0.03f, 0.04f, 0.08f);
        p.fogStart     = 15.0f;
        p.fogEnd       = 40.0f;
        return p;
    }

    case District::Tech: {
        DistrictPalette p;
        p.wallPrimary   = Material(Color3(0.80f, 0.85f, 0.90f), 32.0f, 0.35f);
        p.wallSecondary = Material(Color3(0.70f, 0.75f, 0.80f), 28.0f, 0.30f);
        p.floor         = Material(Color3(0.55f, 0.58f, 0.60f), 20.0f, 0.25f);
        p.trim          = Material(Color3(0.60f, 0.65f, 0.70f), 64.0f, 0.55f);
        p.accent        = Material(Color3(0.20f, 0.90f, 0.60f), 80.0f, 0.65f);
        p.roof          = Material(Color3(0.50f, 0.52f, 0.55f), 16.0f, 0.20f);
        p.metal         = Material(Color3(0.60f, 0.65f, 0.70f), 96.0f, 0.75f);
        p.emissiveColor     = Color3(0.0f, 1.0f, 0.5f);
        p.emissiveIntensity = 0.70f;
        p.ambientColor = Color3(0.04f, 0.045f, 0.05f);
        p.fogColor     = Color3(0.08f, 0.09f, 0.10f);
        p.fogStart     = 25.0f;
        p.fogEnd       = 55.0f;
        return p;
    }

    case District::Industrial: {
        DistrictPalette p;
        p.wallPrimary   = Material(Color3(0.50f, 0.25f, 0.10f), 12.0f, 0.15f);
        p.wallSecondary = Material(Color3(0.42f, 0.22f, 0.10f), 10.0f, 0.12f);
        p.floor         = Material(Color3(0.28f, 0.24f, 0.20f), 8.0f, 0.10f);
        p.trim          = Material(Color3(0.45f, 0.40f, 0.32f), 32.0f, 0.40f);
        p.accent        = Material(Color3(0.90f, 0.60f, 0.15f), 48.0f, 0.50f);
        p.roof          = Material(Color3(0.30f, 0.25f, 0.18f), 8.0f, 0.10f);
        p.metal         = Material(Color3(0.35f, 0.30f, 0.25f), 64.0f, 0.60f);
        p.emissiveColor     = Color3(1.0f, 0.7f, 0.2f);
        p.emissiveIntensity = 0.75f;
        p.ambientColor = Color3(0.03f, 0.025f, 0.02f);
        p.fogColor     = Color3(0.08f, 0.06f, 0.04f);
        p.fogStart     = 18.0f;
        p.fogEnd       = 45.0f;
        return p;
    }

    case District::Slums: {
        DistrictPalette p;
        p.wallPrimary   = Material(Color3(0.40f, 0.30f, 0.20f), 8.0f, 0.08f);
        p.wallSecondary = Material(Color3(0.30f, 0.28f, 0.25f), 6.0f, 0.06f);
        p.floor         = Material(Color3(0.22f, 0.18f, 0.14f), 4.0f, 0.05f);
        p.trim          = Material(Color3(0.35f, 0.28f, 0.20f), 16.0f, 0.20f);
        p.accent        = Material(Color3(0.80f, 0.40f, 0.10f), 32.0f, 0.35f);
        p.roof          = Material(Color3(0.25f, 0.20f, 0.15f), 4.0f, 0.05f);
        p.metal         = Material(Color3(0.30f, 0.28f, 0.25f), 40.0f, 0.45f);
        p.emissiveColor     = Color3(1.0f, 0.5f, 0.0f);
        p.emissiveIntensity = 0.60f;
        p.ambientColor = Color3(0.015f, 0.012f, 0.01f);
        p.fogColor     = Color3(0.05f, 0.035f, 0.025f);
        p.fogStart     = 12.0f;
        p.fogEnd       = 35.0f;
        return p;
    }

    case District::Residential: {
        DistrictPalette p;
        p.wallPrimary   = Material(Color3(0.50f, 0.45f, 0.40f), 14.0f, 0.18f);
        p.wallSecondary = Material(Color3(0.60f, 0.55f, 0.50f), 12.0f, 0.15f);
        p.floor         = Material(Color3(0.35f, 0.32f, 0.28f), 10.0f, 0.12f);
        p.trim          = Material(Color3(0.45f, 0.42f, 0.38f), 24.0f, 0.30f);
        p.accent        = Material(Color3(0.80f, 0.70f, 0.40f), 40.0f, 0.40f);
        p.roof          = Material(Color3(0.35f, 0.28f, 0.22f), 8.0f, 0.10f);
        p.metal         = Material(Color3(0.40f, 0.38f, 0.35f), 48.0f, 0.50f);
        p.emissiveColor     = Color3(1.0f, 0.85f, 0.4f);
        p.emissiveIntensity = 0.50f;
        p.ambientColor = Color3(0.03f, 0.03f, 0.03f);
        p.fogColor     = Color3(0.06f, 0.06f, 0.06f);
        p.fogStart     = 20.0f;
        p.fogEnd       = 50.0f;
        return p;
    }

    case District::Wasteland: {
        DistrictPalette p;
        p.wallPrimary   = Material(Color3(0.60f, 0.55f, 0.50f), 6.0f, 0.08f);
        p.wallSecondary = Material(Color3(0.40f, 0.35f, 0.25f), 4.0f, 0.05f);
        p.floor         = Material(Color3(0.50f, 0.45f, 0.35f), 4.0f, 0.05f);
        p.trim          = Material(Color3(0.48f, 0.44f, 0.38f), 10.0f, 0.12f);
        p.accent        = Material(Color3(0.55f, 0.50f, 0.42f), 8.0f, 0.10f);
        p.roof          = Material(Color3(0.45f, 0.40f, 0.32f), 4.0f, 0.05f);
        p.metal         = Material(Color3(0.38f, 0.35f, 0.30f), 32.0f, 0.35f);
        p.emissiveColor     = Color3(0.0f, 0.0f, 0.0f);
        p.emissiveIntensity = 0.0f;
        p.ambientColor = Color3(0.04f, 0.035f, 0.025f);
        p.fogColor     = Color3(0.12f, 0.10f, 0.07f);
        p.fogStart     = 30.0f;
        p.fogEnd       = 80.0f;
        return p;
    }

    }

    // Fallback — should never be reached
    return getDistrictPalette(District::Downtown);
}
