#pragma once
#include "rendering/framebuffer.h" // Color3
#include "rendering/mesh.h"        // Material

// ============================================================
// city_materials.h  —  Material palette system for ascworld
// ============================================================
// Each district has a named palette of Material presets.
// Materials also carry an optional "emissive" flag: emissive
// surfaces are self-lit (added as a PointLight in world gen).
//
// Usage:
//   auto mat = CityMaterials::downtown().wall;
//   auto neon = CityMaterials::downtown().accent;  // put a PointLight here
// ============================================================

struct DistrictPalette {
    // Building surfaces
    Material wall;       // primary exterior wall
    Material wallDark;   // secondary / darker wall variant
    Material trim;       // window frames, ledges, corners
    Material floor;      // interior floor, sidewalk
    Material roof;       // rooftop surface
    Material metal;      // metal beams, fire-escape, pipes

    // Atmospheric accent (emissive / neon / glow)
    Material accent;     // neon signs, holo-panels (bright, self-lit)
    Material accentDim;  // dim/worn version of accent

    // Street-level
    Material road;       // asphalt
    Material sidewalk;   // pavement / concrete
    Material grime;      // rust, soot, staining

    // Per-district fog/ambient (stored for renderer setup)
    Color3 fogColor;
    Color3 ambientColor;
    float  fogStart;
    float  fogEnd;
};

namespace CityMaterials {

// ── Downtown ───────────────────────────────────────────────
// Dense skyscrapers, glass, steel, cyan neon
inline DistrictPalette downtown() {
    DistrictPalette p;
    p.wall       = Material(Color3(0.18f, 0.20f, 0.24f), 32.f, 0.6f); // dark glass-steel
    p.wallDark   = Material(Color3(0.10f, 0.11f, 0.14f), 16.f, 0.4f);
    p.trim       = Material(Color3(0.30f, 0.35f, 0.40f), 64.f, 0.8f); // polished steel
    p.floor      = Material(Color3(0.14f, 0.15f, 0.18f),  8.f, 0.2f);
    p.roof       = Material(Color3(0.12f, 0.13f, 0.16f), 16.f, 0.3f);
    p.metal      = Material(Color3(0.25f, 0.28f, 0.32f), 48.f, 0.7f);
    p.accent     = Material(Color3(0.00f, 1.00f, 0.95f),  4.f, 0.1f); // cyan neon
    p.accentDim  = Material(Color3(0.00f, 0.35f, 0.32f),  4.f, 0.1f);
    p.road       = Material(Color3(0.08f, 0.08f, 0.09f),  4.f, 0.05f);
    p.sidewalk   = Material(Color3(0.13f, 0.13f, 0.15f),  4.f, 0.08f);
    p.grime      = Material(Color3(0.06f, 0.06f, 0.07f),  4.f, 0.0f);
    p.fogColor   = Color3(0.02f, 0.04f, 0.08f);
    p.ambientColor = Color3(0.04f, 0.06f, 0.10f);
    p.fogStart   = 20.f;
    p.fogEnd     = 60.f;
    return p;
}

// ── Tech District ──────────────────────────────────────────
// Research labs, holo-panels, green/teal
inline DistrictPalette tech() {
    DistrictPalette p;
    p.wall       = Material(Color3(0.14f, 0.18f, 0.16f), 24.f, 0.5f);
    p.wallDark   = Material(Color3(0.08f, 0.10f, 0.09f), 12.f, 0.3f);
    p.trim       = Material(Color3(0.20f, 0.30f, 0.22f), 48.f, 0.7f);
    p.floor      = Material(Color3(0.10f, 0.14f, 0.12f),  8.f, 0.2f);
    p.roof       = Material(Color3(0.10f, 0.13f, 0.11f), 12.f, 0.25f);
    p.metal      = Material(Color3(0.22f, 0.28f, 0.24f), 40.f, 0.6f);
    p.accent     = Material(Color3(0.20f, 1.00f, 0.40f),  4.f, 0.1f); // holo green
    p.accentDim  = Material(Color3(0.06f, 0.30f, 0.12f),  4.f, 0.1f);
    p.road       = Material(Color3(0.08f, 0.09f, 0.08f),  4.f, 0.05f);
    p.sidewalk   = Material(Color3(0.12f, 0.14f, 0.12f),  4.f, 0.08f);
    p.grime      = Material(Color3(0.05f, 0.07f, 0.05f),  4.f, 0.0f);
    p.fogColor   = Color3(0.02f, 0.05f, 0.03f);
    p.ambientColor = Color3(0.03f, 0.07f, 0.04f);
    p.fogStart   = 18.f;
    p.fogEnd     = 50.f;
    return p;
}

// ── Industrial ─────────────────────────────────────────────
// Warehouses, factories, amber/orange
inline DistrictPalette industrial() {
    DistrictPalette p;
    p.wall       = Material(Color3(0.22f, 0.17f, 0.12f), 12.f, 0.2f); // rust-brown
    p.wallDark   = Material(Color3(0.14f, 0.10f, 0.07f),  8.f, 0.15f);
    p.trim       = Material(Color3(0.35f, 0.28f, 0.18f), 24.f, 0.4f);
    p.floor      = Material(Color3(0.16f, 0.13f, 0.09f),  8.f, 0.1f);
    p.roof       = Material(Color3(0.18f, 0.14f, 0.10f),  8.f, 0.15f);
    p.metal      = Material(Color3(0.30f, 0.22f, 0.14f), 32.f, 0.5f);
    p.accent     = Material(Color3(1.00f, 0.55f, 0.05f),  4.f, 0.1f); // amber
    p.accentDim  = Material(Color3(0.35f, 0.18f, 0.02f),  4.f, 0.1f);
    p.road       = Material(Color3(0.09f, 0.08f, 0.07f),  4.f, 0.05f);
    p.sidewalk   = Material(Color3(0.15f, 0.13f, 0.10f),  4.f, 0.08f);
    p.grime      = Material(Color3(0.07f, 0.05f, 0.03f),  4.f, 0.0f);
    p.fogColor   = Color3(0.06f, 0.04f, 0.02f);
    p.ambientColor = Color3(0.08f, 0.05f, 0.02f);
    p.fogStart   = 15.f;
    p.fogEnd     = 45.f;
    return p;
}

// ── Slums ──────────────────────────────────────────────────
// Shanties, makeshift, fire orange / warm
inline DistrictPalette slums() {
    DistrictPalette p;
    p.wall       = Material(Color3(0.24f, 0.19f, 0.14f),  8.f, 0.1f); // weathered
    p.wallDark   = Material(Color3(0.15f, 0.11f, 0.08f),  4.f, 0.08f);
    p.trim       = Material(Color3(0.28f, 0.20f, 0.14f), 12.f, 0.2f);
    p.floor      = Material(Color3(0.13f, 0.10f, 0.07f),  4.f, 0.05f);
    p.roof       = Material(Color3(0.20f, 0.14f, 0.09f),  6.f, 0.1f); // corrugated metal
    p.metal      = Material(Color3(0.26f, 0.18f, 0.10f), 16.f, 0.3f); // rusted
    p.accent     = Material(Color3(1.00f, 0.40f, 0.05f),  4.f, 0.1f); // fire orange
    p.accentDim  = Material(Color3(0.30f, 0.12f, 0.02f),  4.f, 0.1f);
    p.road       = Material(Color3(0.10f, 0.09f, 0.07f),  4.f, 0.04f);
    p.sidewalk   = Material(Color3(0.14f, 0.12f, 0.09f),  4.f, 0.06f);
    p.grime      = Material(Color3(0.08f, 0.06f, 0.04f),  4.f, 0.0f);
    p.fogColor   = Color3(0.06f, 0.04f, 0.02f);
    p.ambientColor = Color3(0.07f, 0.05f, 0.03f);
    p.fogStart   = 12.f;
    p.fogEnd     = 40.f;
    return p;
}

// ── Residential ────────────────────────────────────────────
// Houses, apartments, warm yellow/white
inline DistrictPalette residential() {
    DistrictPalette p;
    p.wall       = Material(Color3(0.26f, 0.24f, 0.20f), 12.f, 0.15f);
    p.wallDark   = Material(Color3(0.16f, 0.14f, 0.12f),  8.f, 0.10f);
    p.trim       = Material(Color3(0.35f, 0.30f, 0.22f), 20.f, 0.3f);
    p.floor      = Material(Color3(0.18f, 0.16f, 0.12f),  8.f, 0.1f);
    p.roof       = Material(Color3(0.20f, 0.16f, 0.10f),  8.f, 0.12f);
    p.metal      = Material(Color3(0.28f, 0.24f, 0.18f), 24.f, 0.4f);
    p.accent     = Material(Color3(1.00f, 0.90f, 0.50f),  4.f, 0.1f); // warm yellow
    p.accentDim  = Material(Color3(0.30f, 0.26f, 0.14f),  4.f, 0.1f);
    p.road       = Material(Color3(0.09f, 0.09f, 0.08f),  4.f, 0.04f);
    p.sidewalk   = Material(Color3(0.16f, 0.15f, 0.13f),  4.f, 0.07f);
    p.grime      = Material(Color3(0.07f, 0.06f, 0.05f),  4.f, 0.0f);
    p.fogColor   = Color3(0.04f, 0.04f, 0.04f);
    p.ambientColor = Color3(0.06f, 0.06f, 0.05f);
    p.fogStart   = 22.f;
    p.fogEnd     = 65.f;
    return p;
}

// ── Wasteland ──────────────────────────────────────────────
// Ruins, bleached concrete, no emissive
inline DistrictPalette wasteland() {
    DistrictPalette p;
    p.wall       = Material(Color3(0.28f, 0.26f, 0.22f),  6.f, 0.05f);
    p.wallDark   = Material(Color3(0.18f, 0.16f, 0.13f),  4.f, 0.04f);
    p.trim       = Material(Color3(0.22f, 0.20f, 0.17f),  8.f, 0.08f);
    p.floor      = Material(Color3(0.20f, 0.18f, 0.15f),  4.f, 0.03f);
    p.roof       = Material(Color3(0.18f, 0.16f, 0.13f),  4.f, 0.04f);
    p.metal      = Material(Color3(0.22f, 0.18f, 0.13f), 12.f, 0.2f); // heavily rusted
    p.accent     = Material(Color3(0.25f, 0.20f, 0.14f),  4.f, 0.0f); // no real emissive
    p.accentDim  = Material(Color3(0.16f, 0.13f, 0.09f),  4.f, 0.0f);
    p.road       = Material(Color3(0.12f, 0.11f, 0.09f),  4.f, 0.02f);
    p.sidewalk   = Material(Color3(0.18f, 0.16f, 0.13f),  4.f, 0.04f);
    p.grime      = Material(Color3(0.09f, 0.08f, 0.06f),  4.f, 0.0f);
    p.fogColor   = Color3(0.08f, 0.07f, 0.06f);
    p.ambientColor = Color3(0.05f, 0.05f, 0.04f);
    p.fogStart   = 10.f;
    p.fogEnd     = 35.f;
    return p;
}

} // namespace CityMaterials
